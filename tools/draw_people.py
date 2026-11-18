"""Pixel clusters for KT's four-way walker. The neutral frame sets the foot pivot."""
from PIL import Image, ImageDraw, ImageOps

COLOURS = dict(K="#483e48", H="#684843", h="#996245", S="#ce875f",
               s="#efb884", L="#ffda9e", T="#346e77", t="#49988f",
               a="#89c5a1", R="#bf504e", r="#ec7756", g="#f6bd67",
               B="#43495c", b="#576378", W="#765341", w="#aa7953")

FRONT = [
    "     KKKKK     ", "   KKhHHhhKK   ", "  KHhhhHHhhHK  ",
    " KHhhhHHhHHhhK ", " KHHhHHhHssHHK ", "KHHHssHssLsHHHK",
    " KHSsssssLssHK ", " KHSsKsssKssHK ", "  KHSssLsssHK  ",
    "   KSSssssSK   ", "     KSSSK     "]
BACK = [
    "     KKKKK     ", "   KKhHHhhKK   ", "  KHhhhHHhhHK  ",
    " KHhhhHHhHHhhK ", " KhHHhhHHhhHHK ", "KHHhHHhhHHhhHHK",
    " KHHhhHHhhHHHK ", " KHHHhhHHhhHHK ", "  KHHHHhhHHHK  ",
    "   KKHHHHHKK   ", "     KSSSK     "]
RIGHT = [
    "     KKKKK     ", "    KhhHHhKK   ", "  KHHhhhHHhhK  ",
    "  KHHhhHHhHhhK ", " KHHHHhHsssHHK ", " KHHhhhHssLsK  ",
    "KHHHHHSHsssLK  ", " KHhhHSSssKssK ", "  KHHHHSssLLK  ",
    "   KHHHSsssK   ", "     KSSSK     "]

def stamp(im, pattern, x, y, palette):
    for row, chars in enumerate(pattern):
        for col,c in enumerate(chars):
            if c!=' ': im.putpixel((x+col,y+row),palette[c])

def walker(palette):
    from PIL import ImageColor
    colors={k:ImageColor.getcolor(v,'RGBA') for k,v in palette.items()}
    sheet=Image.new("RGBA",(96,112))
    for facing in range(4):
        for f,step in enumerate((0,1,0,-1)):
            im=Image.new("RGBA",(24,28))
            d=ImageDraw.Draw(im)
            d.ellipse((4,23,19,26),fill=(44,66,62,64))
            for xx,off in [(8,step),(13,-step)]:
                if facing>1:xx+=off*2
                d.rectangle((xx,18,xx+3,23+off),fill=colors['B'])
                d.line((xx,20,xx,22+off),fill=colors['b'])
                d.rectangle((xx-1,23+off,xx+3,25+off),fill=colors['K'])
                d.line((xx,23+off,xx+3,23+off),fill=colors['w'])
            y=abs(step)
            for xx,off in [(6,step),(16,-step)]:
                d.rectangle((xx,14+y+off,xx+2,18+y+off),fill=colors['T'])
                d.rectangle((xx,18+y+off,xx+2,20+y+off),fill=colors['S'])
                d.point((xx+1,18+y+off),fill=colors['L'])
            stamp(im,["  KrrrrrrK  "," KtrrrrRttK "," KtaRrttatK ",
                      " KttRtttatK "," KttttttttK "," KtattatttK ",
                      " KTTtTTtTTK ","  KKKKKKKK  "],6,12+y,colors)
            if facing==0 or facing>1:
                px=8 if facing==0 else 5
                d.rounded_rectangle((px,14+y,px+6,21+y),radius=1,fill=colors['K'])
                d.rectangle((px+1,16+y,px+5,19+y),fill=colors['w'])
                d.line((px,14+y,px+6,14+y),fill=colors['g'])
                d.point((px+3,17+y),fill=colors['g'])
            stamp(im,[BACK,FRONT,RIGHT,RIGHT][facing],4,2+y,colors)
            if facing==3: im=ImageOps.mirror(im)
            sheet.alpha_composite(im,(f*24,facing*28))
    return sheet

def people(out):
    sheet=walker(COLOURS)
    sheet.save(out/'climber.png')
    # A broad straw brim and indigo apron distinguish the dock carpenter.
    carpenter=walker(dict(COLOURS,t="#6c779a",T="#525778",H="#665c53",r="#e7ac6a",R="#b78758"))
    frame=carpenter.crop((0,28,24,56))
    d=ImageDraw.Draw(frame); d.rectangle((6,4,16,7),fill="#c8955c")
    d.rectangle((7,4,15,5),fill="#f2ca80"); d.rectangle((4,7,18,8),fill="#a5744f")
    d.line((8,6,15,6),fill="#816254");d.rectangle((9,16,14,20),fill="#505b78")
    frame.save(out/'carpenter.png')
    return sheet

def village_cast(out):
    """Three walk cycles. Accessories change the outline even in silhouette."""
    palettes = [
        dict(COLOURS,t="#49688a",T="#394867",r="#d99c52",R="#9d673d"),
        dict(COLOURS,t="#bd5375",T="#853e65",a="#f59e89",H="#413747",
             h="#695064",s="#bc7955",L="#e8aa79",S="#995c49"),
        dict(COLOURS,t="#e1a03e",T="#b46838",a="#ffe198",H="#783e35",
             h="#b76344",r="#347984",R="#2e606c")]
    atlas = Image.new("RGBA",(96,336))
    for who,palette in enumerate(palettes):
        sheet = walker(palette)
        for facing in range(4):
            for frame,bob in enumerate((0,1,0,1)):
                left,top=24*frame,28*facing
                im=sheet.crop((left,top,left+24,top+28))
                if facing==3: im=ImageOps.mirror(im)
                d=ImageDraw.Draw(im)
                def polygon(pts,colour):
                    d.polygon([(x,y+bob) for x,y in pts],fill=colour)
                if who==0:
                    polygon([(3,7),(7,6),(8,2),(15,2),(17,6),(21,7),(20,9),(4,9)],"#805344")
                    polygon([(5,7),(8,6),(9,3),(15,3),(16,6),(19,7)],"#edbd6b")
                    d.line((8,6+bob,16,6+bob),fill="#976142")
                    if facing==1:
                        d.rectangle((9,16+bob,15,21+bob),fill="#344968")
                        d.line((10,17+bob,14,17+bob),fill="#b0c4ba")
                    d.line((17,17+bob,19,21+bob),fill="#966043",width=2)
                    d.line((16,17+bob,19,15+bob),fill="#8293a0",width=2)
                elif who==1:
                    d.ellipse((2,1+bob,9,8+bob),fill="#453648")
                    d.arc((2,1+bob,8,7+bob),175,290,fill="#8b6682")
                    polygon([(6,6),(9,5),(8,8),(6,9)],"#ffcc65")
                    if facing!=0:
                        polygon([(8,15),(14,15),(17,22),(6,22)],"#97496d")
                        polygon([(9,16),(13,16),(15,21),(8,21)],"#eda0a0")
                    d.arc((15,16+bob,22,23+bob),180,360,fill="#986442")
                    polygon([(15,20),(22,20),(21,24),(16,24)],"#c88749")
                    d.line((16,20+bob,21,20+bob),fill="#ffcd77")
                else:
                    polygon([(4,6),(5,3),(9,1),(15,1),(18,4),(19,6),(16,6),(14,4),(7,6)],"#397f85")
                    d.line((7,4+bob,13,2+bob),fill="#83c8af")
                    if facing!=0:
                        d.line((7,14+bob,15,21+bob),fill="#714954",width=2)
                        d.rectangle((13,18+bob,19,22+bob),fill="#aa5b40")
                        d.line((14,18+bob,19,18+bob),fill="#e69a58")
                    polygon([(7,12),(5,15),(1,15),(5,12)],"#45a999")
                if facing==3: im=ImageOps.mirror(im)
                atlas.paste(im,(left,112*who+top))
    atlas.save(out/'bay-villagers.png')
    # Flick's swallowtail wings have a dark rim, pale eye spots and split tips.
    moth=Image.new("RGBA",(64,52))
    for gold in (0,1):
        for fold in (0,1):
            im=Image.new("RGBA",(32,26));d=ImageDraw.Draw(im)
            rim="#544567"; base=("#5bb1bf","#edaa48")[gold]
            light=("#a6e2d6","#ffe589")[gold]
            wing=([(1,3),(4,3),(9,5),(14,10),(15,15),(11,14),(8,16),(5,13),(3,10)]
                  if not fold else [(7,1),(10,2),(13,6),(15,12),(14,16),(10,13),(8,9)])
            d.polygon(wing,fill=rim)
            inner=([(3,5),(6,5),(12,9),(14,13),(9,12),(7,14),(5,10)]
                   if not fold else [(9,3),(11,5),(14,10),(13,13),(10,9)])
            d.polygon(inner,fill=base)
            x=6 if not fold else 10
            d.line((x,6,x+4,10),fill=light,width=2)
            d.point((x,6),fill="#fff4cf")
            d.polygon([(6,14),(10,13),(14,14),(13,18),(9,20),(6,23),(7,18),(5,18)],fill=rim)
            d.polygon([(8,15),(12,14),(12,17),(8,19)],fill="#d78099")
            im.alpha_composite(ImageOps.mirror(im))
            d=ImageDraw.Draw(im)
            d.line((14,9,12,6,11,5),fill=rim);d.line((17,9,19,6,20,5),fill=rim)
            d.ellipse((13,9,18,14),fill=rim);d.line((15,15,16,20),fill=rim,width=2)
            d.point((14,11),fill="#fff2cc");d.point((17,11),fill="#fff2cc")
            d.line((15,14,16,14),fill="#e2a466")
            moth.paste(im,(fold*32,gold*26))
    moth.save(out/'wind-moth.png')



if __name__=="__main__":
    from pathlib import Path
    village_cast(Path(__file__).resolve().parents[1]/"shell/assets")
