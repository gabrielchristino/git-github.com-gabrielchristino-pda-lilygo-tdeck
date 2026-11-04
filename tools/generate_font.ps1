param([string]$Destination = "$PSScriptRoot/../src/ui/font_pt.c")
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Text;
using System.Text;
public static class PdaFontBuilder {
    public static string Generate() {
        var bitmap = new StringBuilder();
        var descriptors = new StringBuilder("    {0,0,0,0,0,0},\n");
        using (var font = new Font("DejaVu Sans", 14, FontStyle.Regular, GraphicsUnit.Pixel))
        using (var image = new Bitmap(40, 40))
        using (var graphics = Graphics.FromImage(image)) {
            graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
            var format = (StringFormat)StringFormat.GenericTypographic.Clone();
            format.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
            int offset = 0;
            for (int code = 32; code <= 256; ++code) {
                if (code >= 127 && code < 160) continue;
                int scalar = code == 256 ? 8230 : code;
                string text = char.ConvertFromUtf32(scalar);
                graphics.Clear(Color.Transparent);
                graphics.DrawString(text, font, Brushes.White, new PointF(0,0), format);
                int left=40, top=40, right=-1, bottom=-1;
                for (int y=0; y<40; ++y) for (int x=0; x<40; ++x)
                    if (image.GetPixel(x,y).A >= 8) { left=Math.Min(left,x); top=Math.Min(top,y); right=Math.Max(right,x); bottom=Math.Max(bottom,y); }
                int width = Math.Max(0, right-left+1), height = Math.Max(0, bottom-top+1);
                int advance = (int)Math.Round(graphics.MeasureString(text,font,40,format).Width*16);
                if (width == 0) { left=0; top=14; }
                descriptors.AppendFormat("    {{{0},{1},{2},{3},{4},{5}}}, /* U+{6:X4} */\n", offset,advance,width,height,left,14-(top+height),scalar);
                int pending=-1;
                bitmap.AppendFormat("\n    /* U+{0:X4} */ ",scalar);
                for (int y=top; y<top+height; ++y) for (int x=left; x<left+width; ++x) {
                    int alpha=(image.GetPixel(x,y).A+8)/17;
                    if (pending<0) pending=alpha;
                    else { bitmap.AppendFormat("0x{0:X2},",pending*16+alpha); ++offset; pending=-1; }
                }
                if (pending>=0) { bitmap.AppendFormat("0x{0:X2},",pending*16); ++offset; }
            }
        }
        return "/* Generated from DejaVu Sans. See THIRD_PARTY.md. */\n#include <lvgl.h>\n" +
            "static const uint8_t bitmap[] = {"+bitmap+"\n};\n" +
            "static const lv_font_fmt_txt_glyph_dsc_t glyphs[] = {\n"+descriptors+"};\n" +
            "static const lv_font_fmt_txt_cmap_t cmaps[] = {\n" +
            "{32,95,1,NULL,NULL,0,LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},\n" +
            "{160,96,96,NULL,NULL,0,LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},\n" +
            "{8230,1,192,NULL,NULL,0,LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}};\n" +
            "static lv_font_fmt_txt_glyph_cache_t cache;\n" +
            "static const lv_font_fmt_txt_dsc_t descriptor = {.glyph_bitmap=bitmap,.glyph_dsc=glyphs,.cmaps=cmaps,.kern_dsc=NULL,.kern_scale=0,.cmap_num=3,.bpp=4,.kern_classes=0,.bitmap_format=0,.cache=&cache};\n" +
            "const lv_font_t pda_font = {.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=18,.base_line=4,.subpx=LV_FONT_SUBPX_NONE,.underline_position=-2,.underline_thickness=1,.dsc=&descriptor,.fallback=&lv_font_montserrat_14};\n";
    }
}
'@
[PdaFontBuilder]::Generate() | Set-Content -LiteralPath $Destination -Encoding utf8
