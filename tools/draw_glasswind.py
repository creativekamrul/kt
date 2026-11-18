"""Original indexed-colour landscapes and the moonkite's four wing poses."""
from pathlib import Path
from random import Random
from math import sin
from PIL import Image, ImageDraw

out=Path(__file__).resolve().parents[1]/"shell/assets"
rng=Random(611)
im=Image.new("RGB",(384,420),"#494d79")
d=ImageDraw.Draw(im)
mask=Image.new("L",im.size);m=ImageDraw.Draw(mask)
west=[(192,351),(138,335),(90,307),(62,268),(98,237),(82,199),(130,174),(158,153),(192,137)]
east=[(192,351),(237,333),(284,353),(322,317),(310,282),(266,253),(300,219),(274,188),(233,162),(192,137)]
def band(g,pts,width,colour):
    g.line(pts,width=width,fill=colour,joint="curve")
    r=width//2
    for x,y in pts:g.ellipse((x-r,y-r,x+r,y+r),fill=colour)
for y in range(420):
    t=y/420
    d.line((0,y,383,y),fill=(int(60+43*t),int(67+28*t),int(117+27*t)))
for _ in range(330):
    x=rng.randrange(384);y=rng.randrange(420)
    d.point((x,y),fill="#aeb7d2")
for y in [25,110,190,295,376]:
    for i in range(18):
        x=rng.randrange(-20,385);yy=y+rng.randrange(-8,10)
        d.line((x,yy,x+rng.randrange(10,38),yy),fill="#8388ac",width=2)
for pts,w,soil in [(west,24,"#94b7be"),(east,19,"#ba9fae"),([(192,408),(192,351)],28,"#a9aabc"), ([(192,137),(192,28)],19,"#a9aabc")]:
    for dy,off,c in [(15,16,"#424867"),(9,14,"#615a7e"),(4,12,"#847494"),(0,10,"#777e9c"),(0,5,"#a8c5c3"),(0,0,soil)]:
        band(d,[(x,y+dy) for x,y in pts],w+off,c)
    band(m,pts,w+3,255)
for box,c in [((97,47,287,160),"#514e71"),((97,42,287,152),"#867694"),((99,39,285,147),"#9da4be"),((105,44,279,143),"#abb2c9")]:
    d.ellipse(box,fill=c)
m.ellipse((102,42,282,146),fill=255)
for _ in range(3800):
    x=rng.randrange(384);y=rng.randrange(420)
    if mask.getpixel((x,y)) and rng.randrange(3)==0:
        d.line((x,y,x+2,y),fill=rng.choice(["#a8b7c8","#adadb9","#c0c9cf"]))
def prism(x,y,h):
    d.polygon([(x-4,y),(x-5,y-h+5),(x-1,y-h),(x+3,y-h+3),(x+4,y-2)],fill="#5f7298")
    d.polygon([(x-3,y-2),(x-4,y-h+5),(x-1,y-h+1),(x,y-h+5),(x,y-2)],fill="#94e7d5")
    d.line((x,y-h+5,x+2,y-h+3,x+2,y-3),fill="#76b9c6")
for x,y in [(114,325),(49,273),(84,222),(63,199),(144,166),(111,70),(255,56),(267,128)]:
    for j in range(3):prism(x+5*j,y+(j%2)*3,8+rng.randrange(12))
for x,y in [(153,339),(74,292),(87,251),(105,193)]:
    d.ellipse((x-7,y-3,x+8,y+2),fill="#598eaa");d.line((x-4,y-2,x+3,y-2),fill="#a4ebdf")
for x,y in east[1:-1]:
    d.line((x-9,y-1,x-9,y-18,x-2,y-18),fill="#73566d",width=2)
    d.line((x-3,y-18,x-3,y-13),fill="#edd3a8")
    d.polygon([(x-5,y-13),(x-1,y-13),(x,y-9),(x-6,y-9)],fill="#f4b867")
    d.point((x-3,y-8),fill="#fff2b4")
for x,y in [(175,43),(206,44),(177,381)]:
    d.rectangle((x,y-16,x+1,y),fill="#625772")
    d.polygon([(x+2,y-16),(x+13,y-15),(x+9,y-10),(x+2,y-10)],fill="#f5c575")
# The old observatory still spans the ravine on narrow stone piers.
for x,y,h in [(120,331,31),(65,272,23),(101,190,28),(304,317,34),(274,191,26)]:
    for k in range(h):
        for dx in range(6):
            xx,yy=x+dx,y+k
            if mask.getpixel((xx,yy))==0 and im.getpixel((xx,yy)) not in [(168,197,195),(119,126,156)]:
                d.point((xx,yy),fill=("#837797" if dx<2 else "#665e83"))
# Heather and wind-worn slabs cluster on exposed rims.
for x,y in [(130,331),(81,289),(85,215),(121,174),(228,328),(306,301),(278,205),(247,171),(137,124)]:
    for _ in range(10):
        xx=x+rng.randrange(-7,8); yy=y+rng.randrange(-3,4)
        d.line((xx,yy,xx-2,yy-5),fill="#68869b")
        d.line((xx-1,yy-3,xx+1,yy-6),fill="#ac85b6")
        d.point((xx+1,yy-6),fill="#e8b1ce")
# A shallow reflecting pool and a freestanding chime identify the interactive stops.
d.ellipse((86,232,105,241),fill="#6c8aa1");d.ellipse((88,232,103,238),fill="#66b7ba")
d.line((91,235,97,235),fill="#c0f1de")
for x in (254,276):d.line((x,252,x,233),fill="#765c72",width=2)
d.line((252,233,265,230,278,233),fill="#9e7182",width=2)
for x,h in [(259,8),(264,12),(269,9)]:
    d.line((x,233,x,236),fill="#ead2ae");d.rectangle((x-1,236,x+1,233+h),fill="#f8c375")
# Mosaic of a moon and four inset stars on the roost floor.
d.arc((168,79,213,118),22,155,fill="#96a6bf",width=2)
d.arc((172,78,209,115),28,150,fill="#b9c9d3",width=1)
for x,y in [(146,96),(239,94),(191,64),(191,130)]:
    d.polygon([(x,y-3),(x+2,y),(x,y+3),(x-2,y)],fill="#d7c7ae")
# Peaks far below the skyway give the cloud gaps depth.
for x,y in [(22,153),(355,181),(189,218),(34,358)]:
    poly=[(x-22,y+11),(x-9,y-2),(x-4,y-1),(x+3,y-12),(x+12,y+2),(x+24,y+11)]
    d.polygon(poly,fill="#797c9e");d.polygon([(x-5,y),(x+3,y-12),(x+8,y-3),(x+3,y-5)],fill="#a0a0b7")
im.save(out/"glasswind.png");mask.save(out/"glasswind-walk.png")

atlas=Image.new("RGBA",(4*48,38))
for f,tip in enumerate([3,9,15,20]):
    bird=Image.new("RGBA",(48,38));g=ImageDraw.Draw(bird)
    for mirror in [False,True]:
        def poly(pts,c):
            g.polygon([(47-x if mirror else x,y) for x,y in pts],fill=c)
        poly([(22,15),(14,10),(5,tip),(1,tip-2),(4,tip+6),(2,tip+7),(8,tip+12),(7,tip+13),(16,27),(22,23)],"#444265")
        poly([(21,16),(14,12),(4,tip+1),(8,tip+8),(13,tip+10),(18,23),(22,22)],"#927cb0")
        poly([(17,15),(12,13),(6,tip+3),(10,tip+7),(15,tip+8)],"#edac94")
        poly([(17,16),(12,tip+9),(16,tip+11),(20,18)],"#cdb1ce")
    g.polygon([(17,13),(16,6),(21,9),(26,9),(32,5),(30,15),(32,20),(28,26),(24,34),(20,27),(16,23)],fill="#504360")
    g.polygon([(18,13),(21,10),(24,12),(27,10),(30,12),(28,19),(24,23),(19,18)],fill="#f5deba")
    g.ellipse((19,13,22,17),fill="#58516e");g.ellipse((26,13,29,17),fill="#58516e")
    for xx in (21,27):g.point((xx,14),fill="#fff8c3")
    g.polygon([(22,17),(25,17),(24,20)],fill="#cf854f")
    g.line((19,22,23,26,28,22),fill="#78bfbd",width=2)
    atlas.paste(bird,(48*f,0))
atlas.save(out/"moonkite.png")

# Summit approach above the cloud line, with the path cut into the foreground ridge.
peak=Image.new("RGB",(320,180));g=ImageDraw.Draw(peak)
for y in range(180):
    t=min(1,y/128);g.line((0,y,319,y),fill=(int(109+136*t),int(146+55*t),int(195-36*t)))
g.ellipse((198,23,229,54),fill="#ffdf99")
for y,amp,c in [(78,24,"#9c9eb8"),(98,34,"#83869f"),(114,40,"#666e8a")]:
    pts=[(x,y+rng.randrange(-amp,amp)) for x in range(-15,350,27)]
    g.polygon([(0,180)]+pts+[(340,180)],fill=c)
    for i in range(1,len(pts)-1):
        x,h=pts[i]
        if h<pts[i-1][1] and h<pts[i+1][1]:
            g.polygon([(x-7,h+8),(x,h),(x+8,h+8),(x+1,h+5)],fill="#ded8d7")
for _ in range(75):
    x=rng.randrange(320);y=rng.randrange(109,151);g.line((x,y,x+rng.randrange(4,30),y),fill="#c1b9c3")
g.polygon([(36,180),(57,145),(104,139),(148,112),(191,116),(224,84),(264,85),(280,99),(267,112),(225,133),(212,154),(166,180)],fill="#74697e")
summit=[(75,174),(95,153),(119,155),(153,129),(191,131),(214,114),(237,95)]
band(g,summit,20,"#9e99ac");band(g,summit,14,"#e6e0cf")
for x,y in [(70,166),(144,136),(206,120),(233,96)]:g.line((x,y,x+4,y-2),fill="#b5b0b4",width=2)
peak.save(out/"summit-walk.png")
