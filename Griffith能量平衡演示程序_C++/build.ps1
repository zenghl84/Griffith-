param([switch]$Test)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$compiler = $null
$candidates = @('C:\Dev-Cpp\TDM-GCC-64\bin\g++.exe', 'D:\CppDev\msys64\ucrt64\bin\g++.exe')
$fromPath = Get-Command g++.exe -ErrorAction SilentlyContinue
if ($fromPath) { $candidates = @($fromPath.Source) + $candidates }
foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) { $compiler = $candidate; break }
}
if (-not $compiler) { throw '未找到 g++ 编译器。请安装支持 C++17 的 MinGW-w64 或 Dev-C++。' }
$env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
$common = @('-std=c++17', '-O2', '-Wall', '-Wextra', '-static', '-finput-charset=UTF-8', '-fexec-charset=UTF-8')
Write-Host ('编译器：' + $compiler)
& $compiler @common '-mwindows' '-municode' 'griffith_core.cpp' 'griffith_app.cpp' '-o' 'Griffith.exe' '-lgdiplus' '-lgdi32' '-luser32' '-lcomctl32' '-lcomdlg32' '-lshell32'
if ($LASTEXITCODE -ne 0) { throw '图形程序编译失败。' }
& $compiler @common 'griffith_core.cpp' 'test_griffith_core.cpp' '-o' 'test_griffith_core.exe'
if ($LASTEXITCODE -ne 0) { throw '数值测试程序编译失败。' }
if ($Test) {
    & '.\test_griffith_core.exe'
    if ($LASTEXITCODE -ne 0) { throw '物理一致性测试失败。' }
    $checkDirectory = Join-Path $PSScriptRoot '验证输出'
    $process = Start-Process -FilePath (Join-Path $PSScriptRoot 'Griffith.exe') -ArgumentList @('--smoke-test', ('"' + $checkDirectory + '"')) -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw ('界面检查失败，详见：' + $checkDirectory) }
    Write-Host ('界面检查通过，报告与预览：' + $checkDirectory)
}
Write-Host '编译完成。双击 Griffith.exe 即可运行。'
