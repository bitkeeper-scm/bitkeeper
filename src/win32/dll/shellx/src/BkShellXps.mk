
BkShellXps.dll: dlldata.obj BkShellX_p.obj BkShellX_i.obj
	link /dll /out:BkShellXps.dll /def:BkShellXps.def /entry:DllMain dlldata.obj BkShellX_p.obj BkShellX_i.obj \
		kernel32.lib rpcndr.lib rpcns4.lib rpcrt4.lib oleaut32.lib uuid.lib \

.c.obj:
	cl /c /Ox /DWIN32 /D_WIN32_WINNT=0x0400 /DREGISTER_PROXY_DLL \
		$<

clean:
	@del BkShellXps.dll
	@del BkShellXps.lib
	@del BkShellXps.exp
	@del dlldata.obj
	@del BkShellX_p.obj
	@del BkShellX_i.obj
