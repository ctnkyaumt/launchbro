Get-ChildItem 'C:\Users\user\Desktop\code\launchbro\bin\i18n\*.ini' | ForEach-Object {
    $bytes = [System.IO.File]::ReadAllBytes($_.FullName)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $bom = 'UTF-8 BOM'
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        $bom = 'UTF-16LE BOM'
    } elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) {
        $bom = 'UTF-16BE BOM'
    } else {
        $bom = 'No BOM'
    }
    Write-Host "$($_.Name): $bom  ($($bytes.Length) bytes)"
}

$lngBytes = [System.IO.File]::ReadAllBytes('C:\Users\user\Desktop\code\launchbro\bin\launchbro.lng')
if ($lngBytes.Length -ge 3 -and $lngBytes[0] -eq 0xEF -and $lngBytes[1] -eq 0xBB -and $lngBytes[2] -eq 0xBF) {
    $lngBom = 'UTF-8 BOM'
} elseif ($lngBytes.Length -ge 2 -and $lngBytes[0] -eq 0xFF -and $lngBytes[1] -eq 0xFE) {
    $lngBom = 'UTF-16LE BOM'
} elseif ($lngBytes.Length -ge 2 -and $lngBytes[0] -eq 0xFE -and $lngBytes[1] -eq 0xFF) {
    $lngBom = 'UTF-16BE BOM'
} else {
    $lngBom = 'No BOM'
}
Write-Host "launchbro.lng: $lngBom  ($($lngBytes.Length) bytes)"
