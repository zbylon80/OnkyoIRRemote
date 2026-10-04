# Convert the existing vector artwork to a multiresolution Windows icon.
# Uses the rect/circle subset of SVG used by the firmware icon; no packages.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationCore, WindowsBase
$assets = Join-Path $PSScriptRoot '../OnkyoRemote.Desktop/Assets'
[xml]$svg = Get-Content -LiteralPath (Join-Path $assets 'remote.svg') -Raw
$frames = [System.Collections.Generic.List[object]]::new()
$converter = [System.Windows.Media.BrushConverter]::new()
foreach ($size in @(16, 24, 32, 48, 64, 128, 256)) {
    $visual = [System.Windows.Media.DrawingVisual]::new()
    $drawing = $visual.RenderOpen()
    $drawing.PushTransform([System.Windows.Media.ScaleTransform]::new($size / 192.0, $size / 192.0))
    foreach ($shape in $svg.DocumentElement.ChildNodes) {
        if ($shape.NodeType -ne [System.Xml.XmlNodeType]::Element) { continue }
        $brush = $converter.ConvertFromInvariantString($shape.GetAttribute('fill'))
        $pen = $null
        if ($shape.HasAttribute('stroke')) {
            $pen = [System.Windows.Media.Pen]::new($converter.ConvertFromInvariantString($shape.GetAttribute('stroke')), [double]$shape.GetAttribute('stroke-width'))
        }
        switch ($shape.LocalName) {
            'rect' {
                $rect = [System.Windows.Rect]::new([double]$shape.GetAttribute('x'), [double]$shape.GetAttribute('y'), [double]$shape.GetAttribute('width'), [double]$shape.GetAttribute('height'))
                $radius = [double]$shape.GetAttribute('rx')
                $drawing.DrawRoundedRectangle($brush, $pen, $rect, $radius, $radius)
            }
            'circle' {
                $center = [System.Windows.Point]::new([double]$shape.GetAttribute('cx'), [double]$shape.GetAttribute('cy'))
                $radius = [double]$shape.GetAttribute('r')
                $drawing.DrawEllipse($brush, $pen, $center, $radius, $radius)
            }
            default { throw ('Unsupported SVG shape: ' + $shape.LocalName) }
        }
    }
    $drawing.Pop()
    $drawing.Close()
    $bitmap = [System.Windows.Media.Imaging.RenderTargetBitmap]::new($size, $size, 96, 96, [System.Windows.Media.PixelFormats]::Pbgra32)
    $bitmap.Render($visual)
    $encoder = [System.Windows.Media.Imaging.PngBitmapEncoder]::new()
    $encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bitmap))
    $stream = [System.IO.MemoryStream]::new()
    $encoder.Save($stream)
    $frames.Add([pscustomobject]@{ Size = $size; Bytes = $stream.ToArray() })
    $stream.Dispose()
}
$output = [System.IO.File]::Create((Join-Path $assets 'remote.ico'))
$writer = [System.IO.BinaryWriter]::new($output)
try {
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
    $offset = 6 + 16 * $frames.Count
    foreach ($frame in $frames) {
        $dimension = if ($frame.Size -eq 256) { 0 } else { $frame.Size }
        $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
        $writer.Write([byte]0); $writer.Write([byte]0)
        $writer.Write([uint16]1); $writer.Write([uint16]32)
        $writer.Write([uint32]$frame.Bytes.Length); $writer.Write([uint32]$offset)
        $offset += $frame.Bytes.Length
    }
    foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Bytes) }
}
finally { $writer.Dispose(); $output.Dispose() }
Write-Output 'Generated remote.ico (16-256px) from the existing SVG.'
