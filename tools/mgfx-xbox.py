#!/usr/bin/env python3
"""Build the stock MonoGame effects for the original Xbox.

MonoGame normally compiles its stock effects from HLSL with mgfxc, which needs the DirectX or
OpenGL shader compilers. Neither targets the NV2A, and the only assembler that does ships inside
the console's own XGraphics library. So there is no compile step here at all: this writes the
vs.1.1 and ps.1.1 assembly by hand into an .mgfxo, and Platform/Xbox/Graphics/Shader/Shader.Xbox.cs
hands that text to XGAssembleShader the first time the shader is drawn with.

Everything else in the .mgfxo is the format MonoGame's Effect reader expects, so the stock effects
behave like any other effect: parameters, techniques, and constant buffers all work unchanged.

Usage: python tools/mgfx-xbox.py <output directory>
"""

import os
import struct
import sys

MGFX_VERSION = 11

# Must equal Shader.PlatformProfile() in Platform/Xbox/Graphics/Shader/Shader.Xbox.cs. Effect
# rejects a file whose profile does not match, which is what keeps a dx11 .mgfxo off the console.
XBOX_PROFILE = 2

# EffectParameterClass
SCALAR, VECTOR, MATRIX, OBJECT = 0, 1, 2, 3
# EffectParameterType
SINGLE, TEXTURE2D = 3, 7
# VertexElementUsage
POSITION, COLOR, TEXCOORD, NORMAL = 0, 1, 2, 3

# The input registers each usage is assigned. A vertex shader lists these in the .mgfxo and
# XboxVertexDeclaration.Build reads them back to place the stream elements, so the assembly below
# and the declaration built at run time agree without either knowing about the other.
V_POSITION, V_NORMAL, V_COLOR, V_TEXCOORD = 0, 1, 2, 3

# XNA's Color packs red in the low byte, while D3DVSDT_D3DCOLOR reads the low byte as blue, so a
# colour arrives in the register with red and blue traded. Reading it back through this swizzle
# costs nothing, whereas byte-swapping every vertex on the way into the buffer would not.
COLOR_SWIZZLE = '.zyxw'

# The register the backend publishes sampler 0's texture coordinate scale in. The sampler reads a
# swizzled texture from 0 to 1 but a linear one in texels, so every texture coordinate is scaled by
# a value the backend supplies rather than being used as it arrives. Stage i uses this register
# plus i; only stage 0 is sampled here. Must match XboxFormat.TexCoordScaleRegister.
#
# This sits just past BasicEffect's constant buffer, which is the larger of the two and ends at c25.
# The top of the register file is not used for it: D3D keeps some of those registers for itself.
TEXCOORD_SCALE = 26


class Writer:
    """The little-endian primitives BinaryReader expects on the other side."""

    def __init__(self):
        self.out = bytearray()

    def u8(self, v):
        self.out.append(v & 0xFF)

    def boolean(self, v):
        self.u8(1 if v else 0)

    def i16(self, v):
        self.out += struct.pack('<h', v)

    def u16(self, v):
        self.out += struct.pack('<H', v)

    def i32(self, v):
        self.out += struct.pack('<i', v)

    def f32(self, v):
        self.out += struct.pack('<f', v)

    def string(self, v):
        """BinaryWriter's 7-bit encoded length followed by UTF-8."""
        data = v.encode('utf-8')
        n = len(data)
        while n >= 0x80:
            self.u8((n & 0x7F) | 0x80)
            n >>= 7
        self.u8(n)
        self.out += data

    def raw(self, data):
        self.out += data


class Parameter:
    def __init__(self, name, cls, kind, rows, columns, data=None):
        self.name = name
        self.cls = cls
        self.kind = kind
        self.rows = rows
        self.columns = columns
        self.data = data

    def write(self, w):
        w.u8(self.cls)
        w.u8(self.kind)
        w.string(self.name)
        w.string('')        # semantic; nothing here looks parameters up by one
        w.i32(0)            # annotations
        w.u8(self.rows)
        w.u8(self.columns)
        w.i32(0)            # elements: not an array
        w.i32(0)            # struct members
        if self.kind == SINGLE:
            values = self.data or [0.0] * (self.rows * self.columns)
            for v in values:
                w.f32(v)


class Shader:
    def __init__(self, name, stage, source, attributes=(), samplers=(), cbuffer=None):
        self.name = name
        self.stage = stage          # 'vs' or 'ps'
        self.source = source
        self.attributes = attributes
        self.samplers = samplers
        self.cbuffer = cbuffer      # index into the effect's constant buffer list, or None

    def write(self, w, source_file):
        w.boolean(self.stage == 'vs')
        w.string(source_file)
        w.string(self.name)

        # The "bytecode" is the assembly text. The console assembles it; see the module docstring.
        text = self.source.encode('ascii')
        w.i32(len(text))
        w.raw(text)

        w.u8(len(self.samplers))
        for texture_slot, parameter in self.samplers:
            w.u8(0)                 # SamplerType.Sampler2D
            w.u8(texture_slot)
            w.u8(texture_slot)      # sampler slot: the console indexes both by stage
            w.boolean(False)        # no sampler state baked into the effect
            w.string('Texture')
            w.u8(parameter)

        if self.cbuffer is None:
            w.u8(0)
        else:
            w.u8(1)
            w.u8(self.cbuffer)

        w.u8(len(self.attributes))
        for name, usage, index, location in self.attributes:
            w.string(name)
            w.u8(usage)
            w.u8(index)
            w.i16(location)


class ConstantBuffer:
    def __init__(self, name, size, entries):
        self.name = name
        self.size = size
        self.entries = entries      # (parameter index, byte offset)

    def write(self, w):
        w.string(self.name)
        w.i16(self.size)
        w.i32(len(self.entries))
        for parameter, offset in self.entries:
            w.i32(parameter)
            w.u16(offset)


def write_effect(path, source_file, cbuffers, shaders, parameters, techniques):
    """techniques: (name, [(pass name, vs index, ps index)])"""
    w = Writer()
    w.raw(b'MGFX')
    w.u8(MGFX_VERSION)
    w.u8(XBOX_PROFILE)
    # The effect key is what GraphicsDevice.EffectCache is indexed by, so it only has to be stable
    # and distinct per effect rather than a real hash of the content.
    w.i32(sum(ord(c) * (i + 1) for i, c in enumerate(source_file)))

    w.i32(len(cbuffers))
    for cbuffer in cbuffers:
        cbuffer.write(w)

    w.i32(len(shaders))
    for shader in shaders:
        shader.write(w, source_file)

    w.i32(len(parameters))
    for parameter in parameters:
        parameter.write(w)

    w.i32(len(techniques))
    for name, passes in techniques:
        w.string(name)
        w.i32(0)                    # annotations
        w.i32(len(passes))
        for pass_name, vs, ps in passes:
            w.string(pass_name)
            w.i32(0)                # annotations
            w.i32(vs)
            w.i32(ps)
            w.boolean(False)        # no blend state
            w.boolean(False)        # no depth stencil state
            w.boolean(False)        # no rasterizer state

    w.i32(struct.unpack('<i', b'MGFX')[0])      # tail marker the reader checks

    verify(bytes(w.out))
    with open(path, 'wb') as f:
        f.write(w.out)
    return len(w.out)


class Reader:
    """Enough of BinaryReader to walk a file back the way Effect.ReadEffect does."""

    def __init__(self, data, at):
        self.data = data
        self.at = at

    def u8(self):
        self.at += 1
        return self.data[self.at - 1]

    def boolean(self):
        return self.u8() != 0

    def fixed(self, fmt, size):
        self.at += size
        return struct.unpack_from(fmt, self.data, self.at - size)[0]

    def i16(self):
        return self.fixed('<h', 2)

    def u16(self):
        return self.fixed('<H', 2)

    def i32(self):
        return self.fixed('<i', 4)

    def f32(self):
        return self.fixed('<f', 4)

    def string(self):
        length, shift = 0, 0
        while True:
            b = self.u8()
            length |= (b & 0x7F) << shift
            if not b & 0x80:
                break
            shift += 7
        self.at += length
        return self.data[self.at - length:self.at].decode('utf-8')


def verify(data):
    """Re-read a file exactly as MonoGame's Effect does.

    Writing this format by hand is only safe if something checks it, and the cheapest check that
    actually means something is to walk the same fields in the same order and land on the tail
    marker with no bytes left over. A field written at the wrong width shows up here rather than
    as a hang on the console.
    """
    r = Reader(data, 0)
    if data[:4] != b'MGFX':
        raise SystemExit('missing MGFX signature')
    r.at = 4
    version, profile = r.u8(), r.u8()
    if version != MGFX_VERSION or profile != XBOX_PROFILE:
        raise SystemExit('wrong version or profile in header')
    r.i32()                                     # effect key
    r.at = 10                                   # Effect hardcodes a 10 byte header

    for _ in range(r.i32()):                    # constant buffers
        r.string()
        r.i16()
        for _ in range(r.i32()):
            r.i32()
            r.u16()

    for _ in range(r.i32()):                    # shaders
        r.boolean()
        r.string()
        r.string()
        length = r.i32()                        # the assembly text
        r.at += length
        for _ in range(r.u8()):                 # samplers
            r.u8()
            r.u8()
            r.u8()
            if r.boolean():
                raise SystemExit('the verifier does not know the sampler state layout')
            r.string()
            r.u8()
        for _ in range(r.u8()):                 # constant buffer indices
            r.u8()
        for _ in range(r.u8()):                 # vertex attributes
            r.string()
            r.u8()
            r.u8()
            r.i16()

    def parameters():
        count = r.i32()
        for _ in range(count):
            r.u8()
            kind = r.u8()
            r.string()
            r.string()
            if r.i32():
                raise SystemExit('the verifier does not know the annotation layout')
            rows, columns = r.u8(), r.u8()
            elements, members = parameters(), parameters()
            if elements == 0 and members == 0 and kind == SINGLE:
                for _ in range(rows * columns):
                    r.f32()
        return count

    parameters()

    for _ in range(r.i32()):                    # techniques
        r.string()
        r.i32()
        for _ in range(r.i32()):                # passes
            r.string()
            r.i32()
            r.i32()
            r.i32()
            for _ in range(3):                  # blend, depth stencil, rasterizer
                if r.boolean():
                    raise SystemExit('the verifier does not know the render state layout')

    if r.i32() != struct.unpack('<i', b'MGFX')[0]:
        raise SystemExit('the tail marker did not land where the reader expects it')
    if r.at != len(data):
        raise SystemExit('%d bytes left over after the tail marker' % (len(data) - r.at))


# ---------------------------------------------------------------------------- SpriteEffect

SPRITE_VS = """\
vs.1.1
; MatrixTransform occupies c0-c3, transposed, so a column per register.
dp4 oPos.x, v%(pos)d, c0
dp4 oPos.y, v%(pos)d, c1
dp4 oPos.z, v%(pos)d, c2
dp4 oPos.w, v%(pos)d, c3
mov oD0, v%(color)d%(swizzle)s
mul oT0.xy, v%(tex)d, c%(scale)d
""" % {'pos': V_POSITION, 'color': V_COLOR, 'tex': V_TEXCOORD, 'swizzle': COLOR_SWIZZLE,
       'scale': TEXCOORD_SCALE}

SPRITE_PS = """\
ps.1.1
tex t0
mul r0, t0, v0
"""


def build_sprite_effect(out_dir):
    parameters = [
        Parameter('Texture', OBJECT, TEXTURE2D, 0, 0),
        Parameter('MatrixTransform', MATRIX, SINGLE, 4, 4,
                  [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
    ]
    cbuffers = [ConstantBuffer('SpriteVS', 64, [(1, 0)])]
    shaders = [
        Shader('SpriteVertexShader', 'vs', SPRITE_VS, cbuffer=0, attributes=[
            ('Position0', POSITION, 0, V_POSITION),
            ('Color0', COLOR, 0, V_COLOR),
            ('TexCoord0', TEXCOORD, 0, V_TEXCOORD),
        ]),
        Shader('SpritePixelShader', 'ps', SPRITE_PS, samplers=[(0, 0)]),
    ]
    techniques = [('SpriteBatch', [('P0', 0, 1)])]
    return write_effect(os.path.join(out_dir, 'SpriteEffect.xbox.mgfxo'),
                        'SpriteEffect.xbox', cbuffers, shaders, parameters, techniques)


# ---------------------------------------------------------------------------- BasicEffect
#
# The vertex register layout matches the _vs(cN) assignments in upstream's BasicEffect.fx, so the
# assembly below can be read next to the HLSL it replaces.
#
#   c0  DiffuseColor            c13 EyePosition
#   c1  EmissiveColor           c14 FogVector
#   c2  SpecularColor           c15-c18 WorldViewProj
#   c3  SpecularPower           c19-c22 World
#   c4-c12 the three lights     c23-c25 WorldInverseTranspose
#
# The pixel shader only needs FogColor, which upstream also puts at c0.

BASIC_PARAMETERS = [
    Parameter('Texture', OBJECT, TEXTURE2D, 0, 0),
    Parameter('DiffuseColor', VECTOR, SINGLE, 1, 4, [1, 1, 1, 1]),
    Parameter('EmissiveColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('SpecularColor', VECTOR, SINGLE, 1, 3, [1, 1, 1]),
    Parameter('SpecularPower', SCALAR, SINGLE, 1, 1, [16]),
    Parameter('DirLight0Direction', VECTOR, SINGLE, 1, 3, [0, -1, 0]),
    Parameter('DirLight0DiffuseColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('DirLight0SpecularColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('DirLight1Direction', VECTOR, SINGLE, 1, 3, [0, -1, 0]),
    Parameter('DirLight1DiffuseColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('DirLight1SpecularColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('DirLight2Direction', VECTOR, SINGLE, 1, 3, [0, -1, 0]),
    Parameter('DirLight2DiffuseColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('DirLight2SpecularColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('EyePosition', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('FogColor', VECTOR, SINGLE, 1, 3, [0, 0, 0]),
    Parameter('FogVector', VECTOR, SINGLE, 1, 4, [0, 0, 0, 0]),
    Parameter('World', MATRIX, SINGLE, 4, 4,
              [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
    Parameter('WorldInverseTranspose', MATRIX, SINGLE, 4, 3,
              [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0]),
    Parameter('WorldViewProj', MATRIX, SINGLE, 4, 4,
              [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
]

P = dict((parameter.name, i) for i, parameter in enumerate(BASIC_PARAMETERS))

BASIC_VS_CBUFFER = ConstantBuffer('BasicVS', 26 * 16, [
    (P['DiffuseColor'], 0),
    (P['EmissiveColor'], 1 * 16),
    (P['SpecularColor'], 2 * 16),
    (P['SpecularPower'], 3 * 16),
    (P['DirLight0Direction'], 4 * 16),
    (P['DirLight0DiffuseColor'], 5 * 16),
    (P['DirLight0SpecularColor'], 6 * 16),
    (P['DirLight1Direction'], 7 * 16),
    (P['DirLight1DiffuseColor'], 8 * 16),
    (P['DirLight1SpecularColor'], 9 * 16),
    (P['DirLight2Direction'], 10 * 16),
    (P['DirLight2DiffuseColor'], 11 * 16),
    (P['DirLight2SpecularColor'], 12 * 16),
    (P['EyePosition'], 13 * 16),
    (P['FogVector'], 14 * 16),
    (P['WorldViewProj'], 15 * 16),
    (P['World'], 19 * 16),
    (P['WorldInverseTranspose'], 23 * 16),
])

BASIC_PS_CBUFFER = ConstantBuffer('BasicPS', 16, [(P['FogColor'], 0)])


def basic_vertex_shader(lights, texture, vertex_color, fog):
    a = ['vs.1.1']
    a += ['dp4 oPos.%s, v%d, c%d' % (c, V_POSITION, 15 + i)
          for i, c in enumerate('xyzw')]

    if lights:
        # eyeVector = normalize(EyePosition - mul(position, World))
        a += ['dp4 r0.%s, v%d, c%d' % (c, V_POSITION, 19 + i)
              for i, c in enumerate('xyz')]
        a += ['add r1.xyz, c13, -r0',
              'dp3 r2.x, r1, r1',
              'rsq r2.x, r2.x',
              'mul r1.xyz, r1, r2.x']
        # worldNormal = normalize(mul(normal, WorldInverseTranspose))
        a += ['dp3 r3.%s, v%d, c%d' % (c, V_NORMAL, 23 + i)
              for i, c in enumerate('xyz')]
        a += ['dp3 r4.x, r3, r3',
              'rsq r4.x, r4.x',
              'mul r3.xyz, r3, r4.x']

        for i in range(lights):
            direction, diffuse, specular = 4 + 3 * i, 5 + 3 * i, 6 + 3 * i
            # lit does the whole per-light term: r7.y is max(dotL, 0) and r7.z is
            # pow(max(dotH, 0), SpecularPower) gated on dotL being positive, which is what
            # Lighting.fxh spells out as zeroL * dotL and pow(max(dotH, 0) * zeroL, power).
            a += ['add r5.xyz, r1, -c%d' % direction,
                  'dp3 r6.x, r5, r5',
                  'rsq r6.x, r6.x',
                  'mul r5.xyz, r5, r6.x',
                  'dp3 r6.x, -c%d, r3' % direction,
                  'dp3 r6.y, r5, r3',
                  'mov r6.w, c3.x',
                  'lit r7, r6']
            if i == 0:
                a += ['mul r8.xyz, r7.y, c%d' % diffuse,
                      'mul r9.xyz, r7.z, c%d' % specular]
            else:
                a += ['mad r8.xyz, r7.y, c%d, r8' % diffuse,
                      'mad r9.xyz, r7.z, c%d, r9' % specular]

        a += ['mul r8.xyz, r8, c0',
              'add r8.xyz, r8, c1',
              'mul r9.xyz, r9, c2']
        if vertex_color:
            a += ['mul oD0.xyz, r8, v%d%s' % (V_COLOR, COLOR_SWIZZLE),
                  'mul oD0.w, c0.w, v%d.w' % V_COLOR]
        else:
            a += ['mov oD0.xyz, r8',
                  'mov oD0.w, c0.w']
        a += ['mov oD1.xyz, r9']
    else:
        if vertex_color:
            a += ['mul oD0, c0, v%d%s' % (V_COLOR, COLOR_SWIZZLE)]
        else:
            a += ['mov oD0, c0']
        # No oD1.xyz: the unlit pixel shaders never read the specular colour, only the fog
        # factor below, so there is nothing to zero.

    if fog:
        # The output register clamps to 0..1 on the way to the interpolator, which is the
        # saturate in ComputeFogFactor.
        a += ['dp4 oD1.w, v%d, c14' % V_POSITION]

    if texture:
        a += ['mul oT0.xy, v%d, c%d' % (V_TEXCOORD, TEXCOORD_SCALE)]

    return '\n'.join(a) + '\n'


def basic_vertex_attributes(lights, texture, vertex_color):
    attributes = [('Position0', POSITION, 0, V_POSITION)]
    if lights:
        attributes.append(('Normal0', NORMAL, 0, V_NORMAL))
    if vertex_color:
        attributes.append(('Color0', COLOR, 0, V_COLOR))
    if texture:
        attributes.append(('TexCoord0', TEXCOORD, 0, V_TEXCOORD))
    return attributes


def basic_pixel_shader(lit, texture, fog):
    a = ['ps.1.1']
    if texture:
        a += ['tex t0', 'mul r0, t0, v0']
        colour = 'r0'
    else:
        a += ['mov r0, v0']
        colour = 'r0'
    if lit:
        # color.rgb += specular * color.a
        a += ['mad r0.rgb, v1, %s.a, %s' % (colour, colour)]
    if fog:
        # color.rgb = lerp(color.rgb, FogColor * color.a, fogFactor), with the factor riding in
        # the specular alpha the vertex shader wrote.
        a += ['mul r1.rgb, c0, r0.a',
              'lrp r0.rgb, v1.a, r1, r0']
    return '\n'.join(a) + '\n'


def build_basic_effect(out_dir):
    shaders = []
    index = {}

    def vertex(lights, texture, vertex_color, fog):
        key = ('vs', lights, texture, vertex_color, fog)
        if key not in index:
            name = 'VSBasic'
            name += {0: '', 1: 'OneLight', 3: 'VertexLighting'}[lights]
            name += 'Tx' if texture else ''
            name += 'Vc' if vertex_color else ''
            name += '' if fog else 'NoFog'
            index[key] = len(shaders)
            shaders.append(Shader(
                name, 'vs', basic_vertex_shader(lights, texture, vertex_color, fog),
                cbuffer=0, attributes=basic_vertex_attributes(lights, texture, vertex_color)))
        return index[key]

    def pixel(lit, texture, fog):
        key = ('ps', lit, texture, fog)
        if key not in index:
            name = 'PSBasic'
            name += 'VertexLighting' if lit else ''
            name += 'Tx' if texture else ''
            name += '' if fog else 'NoFog'
            index[key] = len(shaders)
            shaders.append(Shader(name, 'ps', basic_pixel_shader(lit, texture, fog),
                                  samplers=[(0, P['Texture'])] if texture else [],
                                  cbuffer=1 if fog else None))
        return index[key]

    # BasicEffect.cs picks its technique by arithmetic rather than by name:
    #
    #   index = 1 if !fog, + 2 if vertex colour, + 4 if textured,
    #           + 8 three-light, + 16 one light, + 24 per-pixel lighting
    #
    # so these have to come out in exactly that order or a title silently gets another shader.
    # The names are upstream's for the sake of anyone comparing against BasicEffect.fx.
    #
    # The per-pixel block runs the three-light vertex shaders. The NV2A pixel pipeline is a
    # register combiner with no way to normalize an interpolated normal, so there is no per-pixel
    # lighting to be had; a title that asks for it still gets lighting, computed per vertex.
    blocks = [('', 0), ('_VertexLighting', 3), ('_OneLight', 1), ('_PixelLighting', 3)]
    techniques = []
    for suffix, lights in blocks:
        for texture in (False, True):
            for vertex_color in (False, True):
                for fog in (True, False):
                    name = ('BasicEffect' + suffix
                            + ('_Texture' if texture else '')
                            + ('_VertexColor' if vertex_color else '')
                            + ('' if fog else '_NoFog'))
                    techniques.append((name, [(
                        'P0',
                        vertex(lights, texture, vertex_color, fog),
                        pixel(lights > 0, texture, fog))]))

    cbuffers = [BASIC_VS_CBUFFER, BASIC_PS_CBUFFER]
    return write_effect(os.path.join(out_dir, 'BasicEffect.xbox.mgfxo'),
                        'BasicEffect.xbox', cbuffers, shaders, BASIC_PARAMETERS, techniques)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    out_dir = sys.argv[1]
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    for name, build in (('SpriteEffect', build_sprite_effect),
                        ('BasicEffect', build_basic_effect)):
        print('%s.xbox.mgfxo: %d bytes' % (name, build(out_dir)))


if __name__ == '__main__':
    main()
