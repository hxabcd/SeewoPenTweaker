$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $root 'bin\Release\native-cpp'
$resourceObject = Join-Path $out 'SeewoPenTweaker-res.o'
$executable = Join-Path $out 'SeewoPenTweaker.exe'

New-Item -ItemType Directory -Force $out | Out-Null
Push-Location $root
try {
    & windres '.\SeewoPenTweaker.rc' -O coff -o $resourceObject
    if ($LASTEXITCODE -ne 0) { throw 'windres 编译资源失败。' }

    & g++ '.\SeewoPenTweaker.cpp' '.\Config.cpp' '.\SettingsWindow.cpp' '.\AboutWindow.cpp' $resourceObject -o $executable -std=c++17 -mwindows -municode -O2 -s -static -static-libgcc -static-libstdc++ -luser32 -lshell32 -ladvapi32 -lwinhttp -lole32 -lruntimeobject -lpropsys -luuid
    if ($LASTEXITCODE -ne 0) { throw 'g++ 编译失败。' }

    $size = (Get-Item $executable).Length
    Write-Host "构建成功: $executable"
    Write-Host "文件大小: $size bytes ($([math]::Round($size / 1KB, 2)) KB)"
}
finally {
    Pop-Location
}
