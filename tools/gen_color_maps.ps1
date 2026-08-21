Add-Type -AssemblyName System.Drawing

function New-ColorMap($path, $cornerColors) {
    $bmp = New-Object System.Drawing.Bitmap 256, 256
    for ($y = 0; $y -lt 256; $y++) {
        for ($x = 0; $x -lt 256; $x++) {
            $tx = $x / 255.0
            $ty = $y / 255.0
            $r = 0.0; $g = 0.0; $b = 0.0
            # bilinear interpolate 4 corners
            $r += $cornerColors[0].R * (1 - $tx) * (1 - $ty)
            $r += $cornerColors[1].R * $tx * (1 - $ty)
            $r += $cornerColors[2].R * (1 - $tx) * $ty
            $r += $cornerColors[3].R * $tx * $ty
            $g += $cornerColors[0].G * (1 - $tx) * (1 - $ty)
            $g += $cornerColors[1].G * $tx * (1 - $ty)
            $g += $cornerColors[2].G * (1 - $tx) * $ty
            $g += $cornerColors[3].G * $tx * $ty
            $b += $cornerColors[0].B * (1 - $tx) * (1 - $ty)
            $b += $cornerColors[1].B * $tx * (1 - $ty)
            $b += $cornerColors[2].B * (1 - $tx) * $ty
            $b += $cornerColors[3].B * $tx * $ty
            $c = [System.Drawing.Color]::FromArgb(255, [int]$r, [int]$g, [int]$b)
            $bmp.SetPixel($x, $y, $c)
        }
    }
    $dir = Split-Path $path -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "saved $path"
}

$outDir = "C:\Users\Administrator\IDEProjects\gta\mcpe-ohos\entry\src\main\resources\rawfile\images\misc"
# corners: [0]=cool&dry [1]=warm&dry [2]=cool&wet [3]=warm&wet
$foliage = @(
    [System.Drawing.Color]::FromArgb(0x9E, 0x9E, 0x4E),
    [System.Drawing.Color]::FromArgb(0x8A, 0xB3, 0x5A),
    [System.Drawing.Color]::FromArgb(0x6A, 0x8A, 0x3C),
    [System.Drawing.Color]::FromArgb(0x48, 0xB5, 0x18)
)
$grass = @(
    [System.Drawing.Color]::FromArgb(0x91, 0xA3, 0x4B),
    [System.Drawing.Color]::FromArgb(0xBE, 0xC4, 0x50),
    [System.Drawing.Color]::FromArgb(0x6B, 0x8F, 0x36),
    [System.Drawing.Color]::FromArgb(0x7C, 0xC0, 0x5A)
)
New-ColorMap (Join-Path $outDir "foliagecolor.png") $foliage
New-ColorMap (Join-Path $outDir "grasscolor.png") $grass