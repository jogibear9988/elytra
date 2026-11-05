$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This action requires native Windows' }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$install) { throw 'Visual Studio C++ tools are required' }
& "$install\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
if (!$?) { throw 'Visual Studio environment setup failed' }
foreach ($name in @('PATH','INCLUDE','LIB','LIBPATH','WindowsSdkDir','WindowsSDKVersion','VCToolsInstallDir','VCINSTALLDIR','VSINSTALLDIR','VSCMD_ARG_TGT_ARCH')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value) { "$name=$value" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append }
}
foreach ($tool in @('clang-cl','cmake','ninja','git','python')) {
    if (!(Get-Command $tool -ErrorAction SilentlyContinue)) { throw "Native prerequisite missing: $tool" }
}
& clang-cl --version
if ($LASTEXITCODE) { throw 'clang-cl could not run' }
& cmake --version
& ninja --version
