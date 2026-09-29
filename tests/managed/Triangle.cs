// Matrices-tutorial triangle: a vertex buffer, DrawPrimitive, model/view/projection
// from XGraphics, and a background that cycles color. Runs after the self-test.
using System;

namespace Rxdk
{
    public static class Triangle
    {
        public static void Show()
        {
            if (!GraphicsDevice.Open())
            {
                RxdkConsole.Write("graphics open failed " + Num(GraphicsDevice.LastStatus) + "\n");
                return;
            }
            RxdkConsole.Write("graphics " + Num(GraphicsDevice.Width) + "x" + Num(GraphicsDevice.Height)
                + (GraphicsDevice.Progressive ? " progressive" : " interlaced")
                + (GraphicsDevice.Widescreen ? " widescreen " : " 4:3 ")
                + Num(GraphicsDevice.RefreshHz) + "\n");

            byte[] verts = new byte[3 * 16];
            Put(verts, 0, -1f, -1f, 0f, 0xFFFF0000u);
            Put(verts, 1, 1f, -1f, 0f, 0xFF0000FFu);
            Put(verts, 2, 0f, 1f, 0f, 0xFFFFFFFFu);
            VertexBuffer buffer = VertexBuffer.Create(verts.Length);
            buffer.SetData(verts);

            float aspect = GraphicsDevice.Width / (float)GraphicsDevice.Height;
            GraphicsDevice.View = Matrix.LookAtLH(new Vector3(0f, 3f, -5f), new Vector3(0f, 0f, 0f), new Vector3(0f, 1f, 0f));
            GraphicsDevice.Projection = Matrix.PerspectiveFovLH(0.78539816f, aspect, 1f, 100f);
            GraphicsDevice.SetRenderState(RenderState.CullMode, (int)Cull.None);
            GraphicsDevice.SetRenderState(RenderState.Lighting, 0);
            GraphicsDevice.SetRenderState(RenderState.ZEnable, 1);
            GraphicsDevice.SetVertexFormat(VertexFormat.Position | VertexFormat.Diffuse);
            GraphicsDevice.SetVertexBuffer(buffer, 16);

            int frame = 0;
            float angle = 0f;
            while (true)
            {
                byte r, g, b;
                int phase = frame % 180;
                if (phase < 60)
                {
                    r = (byte)(phase * 4);
                    g = 0;
                    b = (byte)(255 - phase * 4);
                }
                else if (phase < 120)
                {
                    r = (byte)(255 - (phase - 60) * 4);
                    g = (byte)((phase - 60) * 4);
                    b = 0;
                }
                else
                {
                    r = 0;
                    g = (byte)(255 - (phase - 120) * 4);
                    b = (byte)((phase - 120) * 4);
                }
                GraphicsDevice.Clear(ClearFlags.Target | ClearFlags.Depth, r, g, b, 1f, 0);
                GraphicsDevice.Model = Matrix.RotationY(angle);
                GraphicsDevice.DrawPrimitive(PrimitiveType.TriangleList, 0, 1);
                GraphicsDevice.Present();
                angle += 0.05f;
                frame++;
            }
        }

        static unsafe void Put(byte[] data, int vertex, float x, float y, float z, uint color)
        {
            fixed (byte* p = data)
            {
                float* f = (float*)(p + vertex * 16);
                f[0] = x;
                f[1] = y;
                f[2] = z;
                *(uint*)(f + 3) = color;
            }
        }

        static string Num(int v)
        {
            if (v == 0) return "0";
            bool neg = v < 0;
            uint u = neg ? (uint)(-v) : (uint)v;
            string s = "";
            while (u > 0) { s = (char)('0' + (int)(u % 10)) + s; u /= 10; }
            return neg ? "-" + s : s;
        }
    }
}
