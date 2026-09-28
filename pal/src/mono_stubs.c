/*
 * RXDK-DotNet — bring-up link stubs (AUTO-GENERATED, see scripts + docs/phase1-link-surface.md).
 * Symbols from deliberately-excluded/unused Mono subsystems (sockets, crypto, globalization,
 * dynamic loading, COM marshal, AOT/LLVM) that the interpreter path references symbolically
 * but does not execute pre-corlib. Each is a no-op returning 0 so the runtime links; replace
 * with real implementations as features are enabled. C linkage => signatures need not match.
 */
/* int mini_emit_memcpy(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mini_gc_set_slot_type_from_fp(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_alloc_freg(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_alloc_ireg(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_allocate_stack_slots(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_aot_get_array_helper_from_wrapper(void){return 0;}
int mono_aot_method_hash(void){return 0;}
/* JIT-side queries into an AOT image. No AOT image on Xbox (JIT+interp), so they answer "nothing
 * here": index -1, all predicates false, no readonly-field override. Replaced by real aot-runtime.c
 * lookups if AOT is added later. Args typed as void* (ABI-compatible pointer args on x86 cdecl). */
void *mono_aot_readonly_field_override(void *field){(void)field;return 0;}
int   mono_aot_direct_icalls_enabled_for_method(void *cfg, void *method){(void)cfg;(void)method;return 0;}
int   mono_aot_get_method_index(void *method){(void)method;return -1;}
int   mono_aot_can_enter_interp(void *method){(void)method;return 0;}
/* int mono_bblock_insert_before_ins(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_call_inst_add_outarg_reg(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_cfg_set_exception_invalid_program(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_cominterop_cleanup(void){return 0;}
int mono_cominterop_init(void){return 0;}
int mono_compile_assembly(void){return 0;}
/* int mono_compile_create_var(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_compile_deferred_assemblies(void){return 0;}
int mono_cpu_count(void){return 0;}
/* int mono_cpu_get_data(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_cpu_limit(void){return 0;}
int mono_cpu_usage(void){return 0;}
int mono_debugger_agent_init(void){return 0;}
/* int mono_decompose_op_imm(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* mono_dl_* are now REAL (mono-dl.c + mono-dl-windows.c are compiled). Stubbing mono_dl_open to
 * return 0 disabled the P/Invoke fallback path entirely; the real mono_dl_open consults the
 * fallbacks we register (rxdk_register_pinvoke_fallback). Do NOT re-add these. */
/* int mono_dl_build_path(void){return 0;} */
/* int mono_dl_close(void){return 0;} */
/* int mono_dl_get_executable_path(void){return 0;} */
/* int mono_dl_get_system_dir(void){return 0;} */
/* int mono_dl_open(void){return 0;} */
/* int mono_dl_open_runtime_lib(void){return 0;} */
/* int mono_dl_symbol(void){return 0;} */
int mono_fixup_exe_image(void){return 0;}
int mono_free_bstr(void){return 0;}
/* int mono_get_got_var(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_get_module_file_name(void){return 0;}
int mono_get_module_filename(void){return 0;}
int mono_load_coree(void){return 0;}
int mono_marshal_alloc_co_task_mem(void){return 0;}
int mono_marshal_alloc_hglobal(void){return 0;}
int mono_marshal_free_ccw(void){return 0;}
int mono_marshal_free_co_task_mem(void){return 0;}
int mono_marshal_free_hglobal(void){return 0;}
int mono_marshal_realloc_co_task_mem(void){return 0;}
int mono_marshal_realloc_hglobal(void){return 0;}
int mono_network_cleanup(void){return 0;}
/* int mono_network_get_data(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_network_init(void){return 0;}
/* int mono_networkinterface_list(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_peephole_ins(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_print_ins(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_process_current_pid(void){return 0;}
int mono_process_get_data(void){return 0;}
/* int mono_process_get_name(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int mono_process_list(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_ptr_to_ansibstr(void){return 0;}
int mono_rand_close(void){return 0;}
int mono_rand_init(void){return 0;}
int mono_rand_open(void){return 0;}
int mono_rand_try_get_bytes(void){return 0;}
int mono_string_from_bstr_checked(void){return 0;}
int mono_string_from_bstr_icall_impl(void){return 0;}
int mono_string_to_bstr_impl(void){return 0;}
int mono_string_to_utf8str_impl(void){return 0;}
int mono_threadpool_io_cleanup(void){return 0;}
int mono_threadpool_io_remove_domain_jobs(void){return 0;}
int mono_threads_schedule_background_job(void){return 0;}
/* int mono_varlist_sort(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int mono_w32file_cancel(void){return 0;}
int mono_w32file_cleanup(void){return 0;}
int mono_w32file_close(void){return 0;}
int mono_w32file_copy(void){return 0;}
int mono_w32file_create(void){return 0;}
int mono_w32file_create_directory(void){return 0;}
int mono_w32file_create_pipe(void){return 0;}
int mono_w32file_delete(void){return 0;}
int mono_w32file_find_close(void){return 0;}
int mono_w32file_find_first(void){return 0;}
int mono_w32file_find_next(void){return 0;}
int mono_w32file_flush(void){return 0;}
int mono_w32file_get_attributes(void){return 0;}
int mono_w32file_get_attributes_ex(void){return 0;}
/* int mono_w32file_get_console_error(void){return 0;}  -- now real (win32_supplement.c console I/O) */
/* int mono_w32file_get_console_input(void){return 0;}  -- now real (win32_supplement.c console I/O) */
/* int mono_w32file_get_console_output(void){return 0;}  -- now real (win32_supplement.c console I/O) */
int mono_w32file_get_cwd(void){return 0;}
int mono_w32file_get_disk_free_space(void){return 0;}
int mono_w32file_get_drive_type(void){return 0;}
int mono_w32file_get_file_size(void){return 0;}
int mono_w32file_get_file_system_type(void){return 0;}
int mono_w32file_get_logical_drive(void){return 0;}
/* int mono_w32file_get_type(void){return 0;}  -- now real (win32_supplement.c console I/O) */
int mono_w32file_init(void){return 0;}
int mono_w32file_lock(void){return 0;}
int mono_w32file_move(void){return 0;}
int mono_w32file_read(void){return 0;}
int mono_w32file_remove_directory(void){return 0;}
int mono_w32file_replace(void){return 0;}
int mono_w32file_seek(void){return 0;}
int mono_w32file_set_attributes(void){return 0;}
int mono_w32file_set_cwd(void){return 0;}
int mono_w32file_set_times(void){return 0;}
int mono_w32file_truncate(void){return 0;}
int mono_w32file_unlock(void){return 0;}
/* int mono_w32file_write(void){return 0;}  -- now real (win32_supplement.c console I/O) */
int mono_w32process_cleanup(void){return 0;}
int mono_w32process_init(void){return 0;}
int mono_w32process_signal_finished(void){return 0;}
int mono_win32_handle_tls_callback_type(void){return 0;}
int monoeg_g_dir_close(void){return 0;}
int monoeg_g_dir_open(void){return 0;}
int monoeg_g_dir_read_name(void){return 0;}
int monoeg_g_file_get_contents(void){return 0;}
/* int monoeg_g_file_test(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int monoeg_g_get_current_dir(void){return 0;}
/* int monoeg_g_mkdtemp(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int ves_icall_Mono_Security_Cryptography_KeyPairPersistence_CanSecure(void){return 0;}
int ves_icall_Mono_Security_Cryptography_KeyPairPersistence_IsMachineProtected(void){return 0;}
int ves_icall_Mono_Security_Cryptography_KeyPairPersistence_IsUserProtected(void){return 0;}
int ves_icall_Mono_Security_Cryptography_KeyPairPersistence_ProtectMachine(void){return 0;}
int ves_icall_Mono_Security_Cryptography_KeyPairPersistence_ProtectUser(void){return 0;}
int ves_icall_System_Diagnostics_FileVersionInfo_GetVersionInfo_internal(void){return 0;}
int ves_icall_System_Globalization_CalendarData_fill_calendar_data(void){return 0;}
/* internal_compare/internal_index are now REAL (ordinal collation in win32_supplement.c). The
 * return-0 stubs made CompareInfo report "equal"/"found at 0", breaking String.StartsWith/Compare. */
/* int ves_icall_System_Globalization_CompareInfo_internal_compare(void){return 0;} */
/* int ves_icall_System_Globalization_CompareInfo_internal_index(void){return 0;} */
int ves_icall_System_Globalization_CultureData_fill_culture_data(void){return 0;}
int ves_icall_System_Globalization_CultureData_fill_number_data(void){return 0;}
int ves_icall_System_Globalization_CultureInfo_construct_internal_locale_from_lcid(void){return 0;}
int ves_icall_System_Globalization_CultureInfo_construct_internal_locale_from_name(void){return 0;}
int ves_icall_System_Globalization_CultureInfo_get_current_locale_name(void){return 0;}
int ves_icall_System_Globalization_CultureInfo_internal_get_cultures(void){return 0;}
int ves_icall_System_Globalization_RegionInfo_construct_internal_region_from_name(void){return 0;}
int ves_icall_System_IOSelector_Add(void){return 0;}
int ves_icall_System_IOSelector_Remove(void){return 0;}
int ves_icall_System_IO_MonoIO_DumpHandles(void){return 0;}
int ves_icall_System_IO_MonoIO_get_AltDirectorySeparatorChar(void){return 0;}
int ves_icall_System_IO_MonoIO_get_DirectorySeparatorChar(void){return 0;}
int ves_icall_System_IO_MonoIO_get_PathSeparator(void){return 0;}
int ves_icall_System_IO_MonoIO_get_VolumeSeparatorChar(void){return 0;}
int ves_icall_System_Net_Dns_GetHostByAddr(void){return 0;}
int ves_icall_System_Net_Dns_GetHostByName(void){return 0;}
int ves_icall_System_Net_Dns_GetHostName(void){return 0;}
int ves_icall_System_Net_Sockets_SocketException_WSAGetLastError_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Accept_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Available_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Bind_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Blocking_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Close_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Connect_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Disconnect_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Duplicate_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_GetSocketOption_arr_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_GetSocketOption_obj_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_IOControl_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Listen_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_LocalEndPoint_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Poll_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_ReceiveFrom_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Receive_array_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Receive_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_RemoteEndPoint_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Select_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_SendFile_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_SendTo_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Send_array_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Send_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_SetSocketOption_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Shutdown_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_Socket_icall(void){return 0;}
int ves_icall_System_Net_Sockets_Socket_SupportPortReuse_icall(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_AddRefInternal(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_BufferToBSTR(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_FreeBSTR(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_PtrToStringBSTR(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_QueryInterfaceInternal(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_ReleaseInternal(void){return 0;}
int ves_icall_System_Runtime_InteropServices_Marshal_StringToHGlobalAnsi(void){return 0;}
int ves_icall_System_Security_Principal_WindowsIdentity_GetCurrentToken(void){return 0;}
int ves_icall_System_Security_Principal_WindowsIdentity_GetRoles(void){return 0;}
int ves_icall_System_Security_Principal_WindowsIdentity_GetTokenName(void){return 0;}
int ves_icall_System_Security_Principal_WindowsIdentity_GetUserToken(void){return 0;}
int ves_icall_System_Security_Principal_WindowsImpersonationContext_CloseToken(void){return 0;}
int ves_icall_System_Security_Principal_WindowsImpersonationContext_DuplicateToken(void){return 0;}
int ves_icall_System_Security_Principal_WindowsPrincipal_IsMemberOfGroupId(void){return 0;}
int ves_icall_System_Security_Principal_WindowsPrincipal_IsMemberOfGroupName(void){return 0;}
int ves_icall_System_Text_Normalization_load_normalization_resource(void){return 0;}
int ves_icall_cancel_blocking_socket_operation(void){return 0;}
/* int realloc_code(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
/* int set_code_cursor(void){return 0;}  -- obsolete: real symbol now linked (JIT enabled) */
int inflate(void){return 0;}
int inflateInit2_(void){return 0;}
/* int mono_file_map(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* int mono_file_unmap(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* mono_icall_* below are now REAL (icall-windows.c compiles once shlobj.h/SendMessageTimeout gates
 * are forced OFF in config.h). Stubbing them shadowed the real symbols -> mono_icall_get_new_line
 * returned NULL -> Environment.NewLine null -> System.Console cctor NRE. Do NOT re-add these. */
/* int mono_icall_get_environment_variable_names(void){return 0;} */
/* int mono_icall_get_file_path_prefix(void){return 0;} */
/* int mono_icall_get_machine_name(void){return 0;} */
/* int mono_icall_get_new_line(void){return 0;} */
/* int mono_icall_get_platform(void){return 0;} */
/* int mono_icall_get_windows_folder_path(void){return 0;} */
/* int mono_icall_is_64bit_os(void){return 0;} */
/* int mono_icall_make_platform_path(void){return 0;} */
/* int mono_icall_module_get_hinstance(void){return 0;} */
/* int mono_icall_wait_for_input_idle(void){return 0;} */
/* int mono_icall_write_windows_debug_string(void){return 0;} */
/* mono_jit_init / mono_jit_init_version are now REAL (driver.c compiles) — no stub. */
int mono_mmap_close(void){return 0;}
int mono_mmap_configure_inheritability(void){return 0;}
int mono_mmap_flush(void){return 0;}
int mono_mmap_map(void){return 0;}
int mono_mmap_open_file(void){return 0;}
int mono_mmap_open_handle(void){return 0;}
int mono_mmap_unmap(void){return 0;}
/* int mono_mprotect(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* int mono_pagesize(void){return 0;}  -- now real (mono-mmap-windows.c) */
/* int mono_shared_area(void){return 0;}  -- now real (mono-mmap-windows.c) */
/* int mono_shared_area_for_pid(void){return 0;}  -- now real (mono-mmap-windows.c) */
/* int mono_shared_area_instances(void){return 0;}  -- now real (mono-mmap-windows.c) */
/* int mono_shared_area_unload(void){return 0;}  -- now real (mono-mmap-windows.c) */
/* int mono_valloc(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* int mono_valloc_aligned(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* int mono_valloc_granule(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* int mono_vfree(void){return 0;}  -- now real (mono-mmap-windows.c / win32_supplement.c) */
/* int ves_icall_System_Environment_BroadcastSettingChange(void){return 0;}  -- now real (icall-windows.c) */

/* Symbols newly referenced (cdecl, undecorated) once the real JIT objects are pulled in. GetFileAttributesW
 * here is the cdecl form (distinct from the stdcall @4 thunk in win_cdecl_shims.c); route it to the
 * working ANSI variant so file checks along this path still resolve. _wmktemp/_wopen are wide CRT temp
 * helpers we don't use — fail benignly. */
extern unsigned long __attribute__((__stdcall__)) GetFileAttributesA(const char *path); /* @4 */
unsigned long GetFileAttributesW(const unsigned short *w)
{
    char a[520]; int i = 0;
    if (w) for (; i < 519 && w[i]; ++i) a[i] = (char)w[i];
    a[i] = 0;
    return GetFileAttributesA(a);
}
int _wmktemp(void) { return 0; }
int _wopen(void)   { return -1; }
