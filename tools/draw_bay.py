"""Willow Bay overworld and hiker, drawn directly on KT's pixel grid."""
from pathlib import Path
from math import sin
from random import Random
from PIL import Image, ImageDraw, ImageOps

OUT = Path(__file__).resolve().parents[1] / "shell/assets"
rng = Random(831)
bay = Image.new("RGB", (512, 344), "#88bc69")
d = ImageDraw.Draw(bay)
mask = Image.new("L", bay.size, 255)
block = ImageDraw.Draw(mask)

# A spring meadow has broad patches of related hues. Keep the walking lanes quiet.
for x,y,rx,ry,c in [(59,96,84,47,"#72af68"),(437,83,112,54,"#a2c967"),
                     (332,281,134,76,"#99c56a"),(15,311,153,85,"#67a976")]:
    d.ellipse((x-rx,y-ry,x+rx,y+ry),fill=c)
for _ in range(1300):
    x,y=rng.randrange(512),rng.randrange(344)
    if rng.randrange(5): d.line((x,y,x+2,y),fill=rng.choice(["#99c874","#80b66b"]))
    else:
        d.line((x,y,x-1,y-2),fill="#67a167")
        d.point((x+1,y-2),fill="#b7d476")

# Low terraces above town, framed by an open mountain pass.
def cliff(left,right,y):
    points=[(x,y+int(3*sin(x*.13)+3*sin(x*.047))) for x in range(left,right+1,3)]
    for h,c in [(22,"#52616b"),(18,"#86767e"),(12,"#ad9291"),(5,"#c8ae99"),(1,"#4f925e"),(-2,"#80b864")]:
        d.polygon([(left,0),(right,0)]+[(x,py+h) for x,py in reversed(points)],fill=c)
    for x,py in points[::5]:
        d.line((x,py+9,x-2,py+15),fill="#8a7981")
    block.polygon([(left,0),(right,0)]+[(x,py+19) for x,py in reversed(points)],fill=0)
cliff(0,210,39);cliff(254,512,32)

# The stream starts below the ridge and empties into the bay.
water=[(123,91),(143,91),(146,111),(153,126),(152,142),(160,158),
       (162,181),(175,197),(171,211),(192,230),(222,246),(222,255),(246,276),
       (231,296),(218,303),(210,327),(182,344),(0,344),(0,221),(21,216),
       (48,222),(69,230),(95,226),(111,211),(127,200),(132,183),(129,166),
       (125,150),(132,131),(125,114)]
d.polygon(water,fill="#48b9ae");d.line(water+[water[0]],fill="#f1d59a",width=6)
block.polygon(water,fill=0)
d.polygon([(0,243),(44,242),(87,251),(115,238),(104,262),(123,280),(174,287),(188,309),(166,344),(0,344)],fill="#329ba9")
d.polygon([(0,271),(62,267),(88,278),(94,302),(144,310),(135,344),(0,344)],fill="#348ba4")
for _ in range(850):
    x,y=rng.randrange(512),rng.randrange(344)
    if mask.getpixel((x,y))==0 and bay.getpixel((x,y)) in [(72,185,174),(50,155,169),(52,139,164)]:
        d.line((x,y,x+rng.randrange(2,7),y),fill=rng.choice(["#77cdbc","#60babb","#9dddc8"]))
# Stone spring.
for x,y in [(119,93),(124,87),(142,84),(148,95)]:
    d.polygon([(x-5,y+3),(x-5,y),(x,y-5),(x+5,y-3),(x+6,y+4)],fill="#887f87")
    d.line((x-4,y,x,y-4,x+3,y-3),fill="#c3b4a3")

lanes=[]
def lane(points,width):
    lanes.append((points,width))
lane([(300,342),(300,279),(295,234),(276,198),(266,183),(240,177),(195,169),(161,169)],22)
lane([(277,199),(306,197),(339,202),(369,202),(399,193)],20)
lane([(259,181),(276,145),(275,119),(257,95),(244,74),(232,57),(233,28)],19)
lane([(128,168),(95,165),(79,143),(73,115),(85,88),(111,69),(180,68),(231,52)],15)
lane([(300,258),(273,258),(253,251),(222,246)],13)
# Paint the branches as one path so forks share the same soft bank.
for rim,color in [(4,"#a6ae6c"),(2,"#d3b47b"),(0,"#ebcb8e")]:
    for points,width in lanes:
        w=width+rim
        d.line(points,fill=color,width=w,joint="curve")
        for x,y in points:
            d.ellipse((x-w//2,y-w//2,x+w//2,y+w//2),fill=color)
for _ in range(1000):
    x,y=rng.randrange(511),rng.randrange(343)
    if bay.getpixel((x,y))==(235,203,142):d.line((x,y,x+1,y),fill=rng.choice(["#dfbc85","#f2d9a2"]))
# Wide wharf and tied-up rowboat.
def boards(box):
    x,y,r,b=box
    d.rectangle(box,fill="#795342")
    for xx in range(x,r,5):
        d.rectangle((xx,y,min(xx+3,r),b-2),fill=rng.choice(["#c58a5b","#d49865","#e3ac71"]))
        d.point((xx+1,y+2),fill="#745346")
    block.rectangle(box,fill=255)
boards((168,240,228,252));boards((164,231,177,258))
d.polygon([(161,248),(160,267),(149,279),(144,274),(147,256),(154,248)],fill="#714845")
d.polygon([(158,251),(156,265),(149,273),(148,270),(151,258)],fill="#d48a59")
d.line((149,263,157,266),fill="#eabd86",width=2)
d.line((157,251,168,247),fill="#e2c99b")

# Bridge structure. In-game overlays fill the three missing places as spans are repaired.
boards((119,159,178,179))
for yy in (157,178):
    d.line((116,yy,180,yy),fill="#735042",width=2)
    for xx in (117,178):
        d.rectangle((xx-2,yy-6,xx+1,yy+3),fill="#98613f")
        d.line((xx-2,yy-6,xx+1,yy-6),fill="#e9b675")
# Gold gravel by the moorings and lily pads in the sheltered shallows.
for x,y in [(38,245),(56,260),(83,246),(175,299),(194,280)]:
    for dx,dy,r in [(-3,2,3),(3,-1,2),(0,6,2)]:
        d.ellipse((x+dx-r,y+dy-1,x+dx+r,y+dy+1),fill="#72b78c")
        d.line((x+dx-r,y+dy-1,x+dx,y+dy-1),fill="#b9d697")
    d.point((x+2,y-2),fill="#f2b5aa")
# Stone steps and gate posts.
for i in range(6):
    x,y=219+i//2,27+i*4
    d.rectangle((x,y,245,y+3),fill="#a89a94");d.line((x,y,245,y),fill="#e1ceae")
for x in (209,254):
    d.rectangle((x-3,43,x+3,60),fill="#885d46");d.rectangle((x-3,41,x+3,45),fill="#e9b16a")
d.line((209,42,232,46,254,42),fill="#78534c")
for x,y,c in [(216,44,"#ed7562"),(228,46,"#ffd267"),(240,45,"#398d92")]:
    d.polygon([(x,y),(x+6,y),(x+3,y+6)],fill=c)

foreground = []

def tree(x,y,size,kind):
    im=Image.new("RGBA",(64,76));b=ImageDraw.Draw(im)
    pal={"leaf":("#31735d","#4d9564","#79b663","#b8d877"),
         "blossom":("#9b647b","#cd7892","#ef9dac","#ffd5c2"),
         "willow":("#317969","#439a70","#75bb6f","#acd475")}[kind]
    b.ellipse((7,61,51,70),fill=(45,78,63,80))
    b.polygon([(23,66),(28,55),(28,30),(34,29),(35,59),(40,66),(32,63),(28,67)],fill="#725449")
    b.line((31,59,31,42,39,32),fill="#b78358",width=2)
    clusters=[(13,37,11),(22,29,14),(37,25,13),(46,35,12),(34,40,14),(24,16,12)]
    for cx,cy,r in clusters:
        b.ellipse((cx-r,cy-r,cx+r,cy+r),fill=pal[0])
        b.ellipse((cx-r,cy-r,cx+r-2,cy+r-3),fill=pal[1])
        b.polygon([(cx-r+1,cy),(cx-r+2,cy-5),(cx-3,cy-r+1),(cx+4,cy-r+2),(cx+5,cy-6),(cx-2,cy-5),(cx-5,cy)],fill=pal[2])
        for _ in range(4):
            a,b1=cx+rng.randrange(-7,5),cy+rng.randrange(-7,2)
            b.line((a,b1,a+2,b1-1),fill=pal[3])
    # Broken leaf clusters keep the crown from reading as a stack of discs.
    for _ in range(90):
        xx,yy=rng.randrange(3,55),rng.randrange(4,54)
        pix=im.getpixel((xx,yy))
        if pix[3] and pix[:3] not in [(114,84,73),(183,131,88)]:
            tone=pal[3] if yy<24 else pal[2]
            b.line((xx,yy,xx+2,yy-1),fill=tone)
    if kind=="willow":
        for j in range(4,56,4):
            yy=34+rng.randrange(7)
            b.line((j,yy,j-1,yy+8,j-3,yy+14),fill=pal[1],width=2)
            b.line((j+1,yy,j,yy+5),fill=pal[2])
    im=im.resize((int(64*size),int(76*size)),Image.Resampling.NEAREST)
    foreground.append((im, int(x-32*size), int(y-66*size), y))
    bay.paste(im,(int(x-32*size),int(y-66*size)),im)
    block.ellipse((x-6*size,y-4*size,x+6*size,y+2*size),fill=0)

def flowers(x,y,count,c):
    for _ in range(count):
        xx,yy=x+rng.randrange(-10,11),y+rng.randrange(-4,5)
        d.line((xx,yy+3,xx,yy),fill="#4a885c")
        d.line((xx-1,yy,xx+1,yy),fill=c);d.point((xx,yy-1),fill="#ffe099")
def fence(points):
    for dy in (-6,-3):d.line([(x,y+dy) for x,y in points],fill="#b17e55",width=2)
    for x,y in points:
        d.rectangle((x-1,y-9,x+1,y),fill="#8a604a")
        d.point((x-1,y-9),fill="#f3ca87")
for x,y,size,kind in [(10,64,.94,"leaf"),(58,63,.82,"leaf"),(322,59,.86,"leaf"),
                       (377,54,1,"leaf"),(450,57,.94,"blossom"),(493,83,1.1,"leaf"),
                       (34,146,1.05,"willow"),(17,195,.88,"willow"),(111,130,.65,"willow"),
                       (472,240,.95,"blossom"),(437,274,.83,"blossom"),(489,302,1,"leaf")]:
    tree(x,y,size,kind)
fence([(328,139),(342,140),(356,140)])
fence([(429,202),(444,199),(458,197)])
fence([(362,285),(377,289),(393,291),(410,291)])
for x,y,c in [(47,154,"#f3b763"),(100,201,"#eb9994"),(178,146,"#956fbb"),
              (260,245,"#f2ae52"),(380,214,"#e27974"),(412,214,"#9a77c2"),
              (467,278,"#f7bdd0"),(369,275,"#ebbf56"),(305,99,"#9b7cbf")]:
    flowers(x,y,12,c)

# A timber cottage with a sloping red roof, blue shutters and a covered bench.
d.ellipse((340,171,435,196),fill="#78a263")
d.rectangle((349,139,421,184),fill="#956344")
d.rectangle((353,141,417,181),fill="#f5d9a0")
for x in (350,370,399,418):d.rectangle((x,141,x+2,182),fill="#986847")
d.rectangle((351,180,421,185),fill="#9c8880")
d.rectangle((405,97,412,122),fill="#927675");d.rectangle((403,95,414,99),fill="#c99a8c")
d.polygon([(338,145),(358,109),(402,102),(431,138),(430,145),(349,153)],fill="#71474d")
d.polygon([(340,143),(359,110),(400,105),(378,141)],fill="#ce694d")
d.polygon([(378,141),(400,105),(428,138)],fill="#e59868")
for y in (115,121,127,133,139):
    l=357-(y-111)//2;r=398-(y-107)//2
    d.line((l,y,r,y-4),fill="#ab584c")
    for x in range(l+3,r-2,7):d.line((x,y-1,x+1,y-3),fill="#eca376")
d.rectangle((382,153,395,181),fill="#467b78");d.rectangle((384,156,393,169),fill="#75b6ac")
d.point((392,174),fill="#f9d17d")
for x in (357,403):
    d.rectangle((x-2,151,x+12,167),fill="#5c7772")
    d.rectangle((x,153,x+9,164),fill="#82c8c7")
    d.line((x+4,152,x+4,165),fill="#f6d396")
    d.line((x,159,x+9,159),fill="#f6d396")
    d.rectangle((x-2,165,x+11,169),fill="#a96948")
    for j in range(4):d.point((x+3*j,164-j%2),fill="#f48476")
d.rectangle((378,183,400,187),fill="#d5b187")
block.rectangle((347,140,422,184),fill=0)
roof = Image.new("L", bay.size)
outline = ImageDraw.Draw(roof)
outline.rectangle((349,139,421,185),fill=255)
outline.rectangle((403,95,414,99),fill=255)
outline.rectangle((405,97,412,122),fill=255)
outline.polygon([(338,145),(358,109),(402,102),(431,138),(430,145),(349,153)],fill=255)
cottage = bay.convert("RGBA")
cottage.putalpha(roof)
foreground.append((cottage.crop((337,94,432,186)),337,94,185))
# Blue and cream laundry beside the lodge; tiny details confined to a few spots.
d.line((427,144,451,149,466,146),fill="#796351")
for xx,yy,c in [(431,146,"#fff0c4"),(443,148,"#71b6ba"),(455,148,"#e98d78")]:
    d.rectangle((xx,yy,xx+6,yy+8),fill=c)
for xx,yy in [(192,174),(256,76)]:
    d.rectangle((xx,yy-13,xx+2,yy+1),fill="#876446")
    d.polygon([(xx-7,yy-14),(xx+7,yy-14),(xx+10,yy-11),(xx+7,yy-9),(xx-7,yy-9)],fill="#c48b55")
for x,y in [(190,208),(230,279),(38,216),(116,201)]:
    for j in range(4):
        d.line((x+j*2,y,x+j*2-1,y-5-j%3),fill="#508e64")
        d.point((x+j*2-1,y-6-j%3),fill="#b99556")

# A small kitchen garden: irregular beds, terracotta pots and climbing beans.
for row in range(3):
    yy=231+row*7
    d.polygon([(354,yy),(385,yy-2),(392,yy+1),(359,yy+4)],fill="#997758")
    for j in range(7):
        x=357+j*5; y=yy-j//4
        d.line((x,y+1,x+1,y-2),fill="#467d54")
        d.line((x,y,x-2,y-2),fill="#75aa5b")
        if row==1 and j%2==0:d.point((x+2,y-2),fill="#dc7759")
for x,y in [(344,183),(427,186),(432,187)]:
    d.polygon([(x-3,y-4),(x+3,y-4),(x+2,y+1),(x-2,y+1)],fill="#c77e57")
    for dx in (-2,0,2):
        d.line((x,y-5,x+dx,y-8),fill="#548c60");d.point((x+dx,y-9),fill="#f0ad8e")
# Raised beds and a few worn flagstones lead to the lodge door.
for x,y in [(384,189),(390,191),(377,194)]:
    d.line((x,y,x+4,y-1),fill="#c3ad91")
# Alpine stone shelves around the pass.
for x,y in [(185,60),(267,57),(287,71),(55,81),(326,83),(455,84),(164,92),(207,287)]:
    d.polygon([(x-6,y),(x-4,y-4),(x+1,y-6),(x+5,y-3),(x+6,y),(x+2,y+2)],fill="#958795")
    d.line((x-4,y-4,x+1,y-6,x+4,y-4),fill="#d3c4b0")
bay.save(OUT/"bay-village.png")
mask.save(OUT/"bay-walk.png")


# The renderer reuses these silhouettes when the hiker walks behind a landmark.
atlas = Image.new("RGBA",(max(im.width for im,*_ in foreground),
                          sum(im.height for im,*_ in foreground)))
top = 0
with (OUT/"bay-front.csv").open("w",newline="\n") as manifest:
    for im,x,y,base in sorted(foreground,key=lambda prop:prop[3]):
        atlas.paste(im,(0,top))
        manifest.write(f"{x} {y} {base} {top} {im.width} {im.height}\n")
        top += im.height
atlas.save(OUT/"bay-front.png")
