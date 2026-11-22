$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This action requires native Windows' }
# Fixtures and runtime pipes use UTF-8, independently of the runner's ANSI locale.
"PYTHONUTF8=1" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$install) { throw 'Visual Studio C++ tools are required' }
& "$install\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64
if (!$?) { throw 'Visual Studio environment setup failed' }

# The runner's Visual Studio STL can update independently of its bundled LLVM.
# Verify the pinned upstream LLVM installer before running it.
$llvmVersion = '22.1.8'
$llvmHash = '16e5709785fef73c854646241c4a92c5cd574318d1b33c63330dd7721903e55c'
$llvm = Join-Path $env:RUNNER_TEMP "elytra-llvm-$llvmVersion"
$installer = Join-Path $env:RUNNER_TEMP "LLVM-$llvmVersion-win64.exe"
$asset = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$llvmVersion/LLVM-$llvmVersion-win64.exe"
& curl.exe --fail --location --silent --show-error --retry 3 --output $installer $asset
if ($LASTEXITCODE) { throw 'Pinned LLVM download failed' }
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash -ne $llvmHash) {
    throw 'Pinned LLVM installer digest mismatch'
}
$process = Start-Process -FilePath $installer -ArgumentList "/S /D=$llvm" -Wait -PassThru
if ($process.ExitCode) { throw "Pinned LLVM installation failed (exit $($process.ExitCode))" }
Remove-Item -LiteralPath $installer
$llvmBin = Join-Path $llvm 'bin'
$version = & "$llvmBin\clang-cl.exe" --version
if ($LASTEXITCODE -or $version[0] -notmatch '^clang version 22\.1\.8(?:\s|$)') {
    throw "Expected clang-cl $llvmVersion at $llvmBin"
}
if (!(Test-Path -LiteralPath "$llvmBin\lld-link.exe" -PathType Leaf)) {
    throw 'Pinned LLVM is missing lld-link'
}
$env:PATH = "$llvmBin;$env:PATH"

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
