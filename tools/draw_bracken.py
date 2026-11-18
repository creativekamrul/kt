"""Draw Bracken Rise, its foot collision mask, and the copperhorn's animation frames."""
from pathlib import Path
from random import Random
from math import sin
from PIL import Image, ImageDraw

out = Path(__file__).resolve().parents[1] / "shell/assets"
rng = Random(147)
W,H=384,448
im=Image.new("RGB",(W,H),"#69956f"); d=ImageDraw.Draw(im)
mask=Image.new("L",im.size); m=ImageDraw.Draw(mask)
fern=[(192,363),(123,338),(73,303),(112,270),(70,238),(119,207),(149,185),(160,158),(192,146)]
cairn=[(192,363),(244,336),(275,287),(291,232),(257,183),(225,156),(192,146)]
def band(draw,points,w,c):
    draw.line(points,fill=c,width=w,joint="curve")
    r=w//2
    for x,y in points: draw.ellipse((x-r,y-r,x+r,y+r),fill=c)
# Shelf faces, seams and sunlit caps. The canyon falls away between the two trails.
for row in range(-1,8):
    base=row*73
    rim=[(x,base+int(8*sin(x*.034+row)+4*sin(x*.085-row))) for x in range(-4,W+5,3)]
    for off,fill in [(26,"#536b72"),(21,"#796b79"),(16,"#a47e7a"),(9,"#c39980"),(2,"#608967"),(-3,"#90ba71")]:
        d.polygon([(0,base-39),(W,base-39)]+[(x,y+off) for x,y in reversed(rim)],fill=fill)
    for x,y in rim[::4]:
        d.line((x,y+5,x+2,y+9,x-1,y+15),fill="#ac847c")
water=[(138,-10),(153,51),(148,95),(181,157),(209,199),(190,240),(202,275),(195,312),(217,366),(226,456)]
for w,c in [(21,"#6d8377"),(16,"#368e9d"),(10,"#55bbb9")]:band(d,water,w,c)
for px,py in water[2:-1]:
    for k in range(5): d.line((px-5+k*2,py-4,px-4+k*2,py+7+k%3),fill="#b0e3ce")
# Alpine pines and rhododendrons outside the cleared walking corridors.
pine=Image.open(out/"fir.png").convert("RGBA")
keep=Image.new("L",im.size); borders=ImageDraw.Draw(keep)
for line,width in [(water,24),(fern,37),(cairn,31),([(192,448),(192,360)],42),([(192,145),(192,24)],34)]:
    band(borders,line,width,255)
borders.ellipse((105,49,279,164),fill=255)
for y in range(28,H,28):
    for _ in range(7):
        x=rng.randrange(W);scale=rng.uniform(.37,.68)
        tile=pine.resize((int(pine.width*scale),int(pine.height*scale)),Image.Resampling.NEAREST)
        left=x-tile.width//2;top=y-tile.height
        if keep.crop((left,top,left+tile.width,y)).getbbox():continue
        im.paste(tile,(left,top),tile)
for points,width,grass,path in [(fern,23,"#92be75","#eed39a"),(cairn,18,"#b6b183","#d4b09a"),
                                ([(192,436),(192,363)],28,"#9ab970","#e8c48a"),
                                ([(192,145),(192,32)],19,"#a9bd7e","#dcc39d")]:
    for off,c in [(13,"#7e6c70"),(9,grass),(0,path)]:band(d,points,width+off,c)
    band(m,points,width+6,255)
for b,c in [((107,55,277,161),"#7a6771"),((107,51,277,158),"#abb57c"),
            ((111,55,273,153),"#dfbb92"),((120,61,264,148),"#e4c69d")]:
    d.ellipse(b,fill=c)
m.ellipse((112,55,272,153),fill=255)
for _ in range(2100):
    x=rng.randrange(2,W-4);y=rng.randrange(3,H-4)
    if mask.getpixel((x,y)):
        if rng.randrange(5)==0:d.line((x,y,x+2,y),fill="#c8a98a")
    elif im.getpixel((x,y)) in [(144,186,113),(105,149,111)]:
        d.line((x,y,x+2,y-2),fill="#78a362")
        if x<145 and rng.randrange(4)==0:
            d.rectangle((x-1,y-2,x+2,y-1),fill=rng.choice(["#ed8e9e","#ffd786","#c28fc1"]))
def flag(x,y,color):
    d.rectangle((x,y-16,x+1,y),fill="#875f52")
    d.polygon([(x+2,y-16),(x+13,y-15),(x+8,y-11),(x+2,y-11)],fill=color)
    d.line((x+2,y-16,x+10,y-15),fill="#ffde9b")
for x,y in fern[1:-1:2]:
    flag(x-13,y,"#368c86")
    for j in range(3):
        xx=x+13+j*3;yy=y+j%2
        d.line((xx,yy,xx-3,yy-5),fill="#3f8268",width=2)
        d.line((xx-1,yy-2,xx+2,yy-6),fill="#5f9a60",width=2)
        d.point((xx,yy-2),fill="#f38a8a")
for x,y in cairn[1:-1]:
    for i in range(3):
        yy=y-i*3;rr=5-i
        d.ellipse((x+13-rr,yy-3,x+13+rr,yy+1),fill="#837782")
        d.line((x+13-rr+1,yy-2,x+14,yy-3),fill="#d6b39f")
    flag(x-12,y,"#df8652")
# Split stone treads follow the pitch of the eastern route.
for (ax,ay),(bx,by) in zip(cairn[1:-2],cairn[2:-1]):
    dx,dy=bx-ax,by-ay
    length=(dx*dx+dy*dy)**.5
    for j in range(7,int(length)-3,9):
        xx,yy=ax+dx*j/length,ay+dy*j/length
        px,py=-dy/length,dx/length
        tips=(int(xx-px*6),int(yy-py*6),int(xx+px*5),int(yy+py*5))
        d.line(tips,fill="#ab8e84",width=2)
        d.line(tuple(v-1 if k%2 else v for k,v in enumerate(tips)),fill="#edd1aa")
# Boardwalk at the ravine mouth; the two trails share this short approach.
for yy in range(367,382,4):
    d.rectangle((180,yy,204,yy+2),fill="#bc895c")
    d.line((181,yy,203,yy),fill="#f3c489")
for xx in (179,205):
    d.line((xx,363,xx,382),fill="#88624f",width=2)
for cx,cy in [(106,321),(73,253),(118,221),(138,182)]:
    for j in range(12):
        x=cx+rng.randrange(-6,7);y=cy+rng.randrange(-4,5)
        d.line((x,y,x-2,y-3),fill="#408965",width=2)
        d.line((x-1,y-2,x+2,y-5),fill="#559863",width=2)
        if j%3==0:d.rectangle((x,y-3,x+1,y-2),fill="#f78085")
for y in (411,397,383,47,38): d.line((185,y,199,y),fill="#b39883",width=2)
for x in (174,207):flag(x, 51,"#d76456")
# Small cairn ring frames the guardian's clearing.
for x,y in [(115,101),(130,74),(150,63),(238,66),(260,83),(270,106),(248,136),(133,136)]:
    d.polygon([(x-4,y),(x-2,y-5),(x+2,y-6),(x+5,y-2),(x+4,y+1)],fill="#8a8089")
    d.line((x-2,y-4,x+2,y-5),fill="#cdb7a1")
im.save(out/"bracken-rise.png");mask.save(out/"bracken-walk.png")
# Side-on ram. Copper curls frame the face, with a cream ruff and blue slate coat.
atlas=Image.new("RGBA",(48*4,40))
for frame in range(4):
    ram=Image.new("RGBA",(48,40)); r=ImageDraw.Draw(ram)
    dip=[0,2,4,3][frame]
    r.ellipse((4,31,43,37),fill=(73,62,67,77))
    for x,step in [(10,1),(17,-1),(27,-1),(32,1)]:
        off=step*(2 if frame==2 else 0)
        r.polygon([(x,23),(x+5,23),(x+4+off,34),(x+off,34)],fill="#49495a")
        r.line((x+1+off,31,x+4+off,31),fill="#b39b91")
    r.polygon([(3,15),(8,16),(13,11),(26,10),(34,15),(36,24),(31,29),(12,29),(6,23)],fill="#47445a")
    r.polygon([(6,17),(13,13),(23,12),(32,15),(31,24),(25,26),(12,25)],fill="#587989")
    r.polygon([(9,17),(14,13),(23,13),(26,15),(20,15),(17,18),(12,18),(10,20)],fill="#8db5b4")
    r.polygon([(22,14),(29,10),(36,13),(39,19),(34,26),(29,25),(26,29),(25,24),(21,25)],fill="#edcd99")
    r.polygon([(31,11+dip),(39,12+dip),(43,20+dip),(46,22+dip),(44,26+dip),(38,26+dip),(32,20+dip)],fill="#80564e")
    r.line((39,17+dip,41,17+dip),fill="#ffe99f",width=2)
    r.ellipse((23,3+dip,38,18+dip),fill="#66494b");r.ellipse((24,3+dip,37,16+dip),fill="#d58a53")
    r.arc((24,3+dip,37,16+dip),185,350,fill="#ffcb75",width=3)
    r.arc((28,7+dip,35,14+dip),210,480,fill="#956042",width=2)
    atlas.paste(ram,(frame*48,0))
atlas.save(out/"copperhorn.png")

