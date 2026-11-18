"""Draw KT's original sprites, bay panorama and musical cues for the Windows build."""
from pathlib import Path
import math
import random
import struct
import wave
from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[1] / "shell/assets"
OUT.mkdir(exist_ok=True)
rng = random.Random(4471)

def canvas(w,h):
    im=Image.new("RGBA",(w,h))
    return im,ImageDraw.Draw(im)

def tree(kind):
    im,d=canvas(54,70)
    d.ellipse((5,57,48,65),fill=(48,61,52,68))
    d.polygon([(22,61),(24,40),(29,39),(31,62),(35,63),(29,65),(24,64),(19,65)],fill='#594a40')
    d.line([(26,61),(27,40)],fill='#ac7952',width=2)
    if kind != 'willow':
        snow=kind=='spruce'
        for top,base,w in [(22,57,22),(13,45,18),(5,33,15),(0,22,10)]:
            x=25
            points=[(x,top),(x+w,base-5),(x+w-1,base),(x+w//2,base-2),(x+4,base+2),(x-3,base),(x-w//2,base+1),(x-w,base-3)]
            d.polygon(points,fill='#284d4d')
            d.polygon([(x,top),(x+3,top+7),(x+w-4,base-6),(x+3,base-4),(x-4,base-1),(x-w+1,base-4)],fill='#537f88' if snow else '#398467')
            d.polygon([(x,top),(x+1,top+5),(x-4,base-10),(x-10,base-6),(x-w+3,base-6)],fill='#d9eee2' if snow else '#82b967')
            for j in range(5):
                px=rng.randrange(x-w//2,x+w//2); py=rng.randrange(base-9,base-4)
                d.line((px,py,px+2,py-1),fill='#89a68d' if snow else '#70905e')
    else:
        for x,y,r in [(24,25,19),(13, 30,12),(40,30,11),(27,16,14),(10,37,8),(38,41,9)]:
            d.ellipse((x-r,y-r,x+r,y+r),fill='#476f56')
            d.ellipse((x-r,y-r,x+r-3,y+r-5),fill='#62a466')
            d.ellipse((x-r+2,y-r+1,x+2,y-1),fill='#a5c86d')
        for x in range(4,50,3):
            y=31+int(5*math.sin(x))
            d.line((x,y,x-1,y+rng.randrange(7,17)),fill=rng.choice(['#589360','#7bae61','#bdcf78']),width=2)
    return im
pine=tree('pine'); willow=tree('willow'); spruce=tree('spruce')
for name,img in [('fir',pine),('willow',willow),('spruce',spruce)]:img.save(OUT/(name+'.png'))

cabin,d=canvas(78,68)
d.ellipse((3,54,76,66),fill=(48,61,52,68))
d.rectangle((12,32,66,60),fill='#654d42')
for y in range(32,58,5):
    d.rectangle((13,y,65,y+3),fill='#bc905e');d.line((15,y+1,62,y+1),fill='#dcaf77')
d.rectangle((45,39,56,60),fill='#584438')
for x in (18,33):
    d.rectangle((x,39,x+9,49),fill='#6d5846');d.rectangle((x+1,40,x+7,47),fill='#ffc77c')
    d.line((x+4,40,x+4,48),fill='#926f4e')
d.rectangle((55,5,61,21),fill='#77675e')
d.polygon([(4,33),(21,8),(56,8),(75,33),(74,38),(4,38)],fill='#624a43')
for row in range(5):
    y=9+row*5;l=21-row*3;r=55+row*4
    d.rectangle((l,y,r,y+4),fill=['#c36b53','#dc885d','#bb634f','#b35a4d','#a3534b'][row])
    for x in range(l+2,r-2,7):d.line((x,y+1,x-1,y+3),fill='#e49a65')
d.rectangle((40,60,59,62),fill='#ae936d');cabin.save(OUT/'lodge.png')

from draw_people import people, village_cast
hero=people(OUT)
village_cast(OUT)

# Dusk from the trailhead: stepped ridgelines and their lake reflections.
w,h=480,270
im,d=canvas(w,h)
for y in range(h):
    a=min(1,y/160)
    col=tuple(int(p+(q-p)*a) for p,q in zip((107,147,157),(250,206,147)))
    d.line((0,y,w,y),fill=col)
d.ellipse((309,41,340,72),fill='#ffe4aa')
for i in range(23):
    x=rng.randrange(w);y=rng.randrange(16,89);n=rng.randrange(10,43)
    d.line((x,y,x+n,y),fill='#eac9ab',width=2)
for base,amp,col,highlight in [(115,44,'#9697a2','#c7b7ab'),(125,54,'#757f92','#b1a5a0'),(139,58,'#526e77','#91a096')]:
    pts=[(-30,base)]
    for x in range(-25,520,20):pts.append((x,base-rng.randrange(2,amp)))
    d.polygon(pts+[(500,170),(-30,170)],fill=col)
    for i in range(2,len(pts)-1):
        x,y=pts[i]
        if pts[i-1][1]>y and pts[i+1][1]>y:
            d.polygon([(x,y),(x+11,(y+pts[i+1][1])//2),(x+5,y+8),(x+1,y+5),(x-8,y+14)],fill=highlight)
for y in range(151,246):
    d.line((0,y,w,y),fill=('#8fb1a3' if y<174 else '#75a59d'))
    for i in range(10):
        x=rng.randrange(w); d.line((x,y,x+rng.randrange(2,20),y),fill=rng.choice(['#aec2a8','#65938e','#e6c595']))
for side in (0,1):
    for i in range(24):
        x=(i*9-60) if side==0 else (326+i*9)
        y=int(159+(max(0,142-x) if side==0 else max(0,x-330))*.28)
        spr=pine.resize((23,35),Image.Resampling.NEAREST);im.alpha_composite(spr,(x,y-35))
d.polygon([(0,200),(76,187),(142,191),(200,204),(222,219),(306,222),(385,194),(479,182),(480,270),(0,270)],fill='#596f52')
d.polygon([(95,270),(156,232),(175,221),(157,211),(181,213),(202,220),(195,228),(151,270)],fill='#c39967')
for i in range(590):
    x=rng.randrange(w);y=rng.randrange(208,270)
    if im.getpixel((x,y))[:3] == (89,111,82):d.line((x,y,x+1,y-2),fill=rng.choice(['#84915c','#4b6952','#ad9e62']))
im.alpha_composite(cabin,(56,151))
for x,y in [(-18,155),(7,167),(382,143),(407,157),(448,172)]:im.alpha_composite(willow if x<10 else pine,(x,y))
im.convert('RGB').save(OUT/'willow-bay.png')

for name,notes in [('step',(392,494,587)),('camp',(392,494,587,784)),('pickup',(523,659))]:
    rate=22050;samples=[]
    for hz in notes:
        for i in range(2500):
            t=i/rate;env=min(1,i/65)*math.exp(-t*33)
            samples.append(int(5200*env*(math.sin(2*math.pi*hz*t)+.25*math.sin(4*math.pi*hz*t))))
    with wave.open(str(OUT/(name+'.wav')),'wb') as f:
        f.setparams((1,2,rate,0,'NONE','not compressed'));f.writeframes(struct.pack('<'+'h'*len(samples),*samples))

# A quiet six-bar trail theme, played on plucked strings with a flute answer.
rate=22050
beat=.48
duration=6*8*beat
from array import array
mix=array('f',[0]) * int(duration*rate)
def voice(start,pitch,beats,volume,flute=False):
    freq=440*2**((pitch-69)/12)
    count=int(beats*beat*rate)
    for i in range(count):
        t=i/rate;phase=2*math.pi*freq*t
        if flute:
            env=min(1,t*9)*min(1,(count-i)/rate*6)
            tone=math.sin(phase+.08*math.sin(t*29))+.13*math.sin(2*phase)
        else:
            env=min(1,t*90)*math.exp(-3.4*t)*min(1,(count-i)/rate*12)
            tone=math.sin(phase)+.36*math.sin(2*phase)+.17*math.sin(3*phase)
        j=(int(start*rate)+i)%len(mix)
        mix[j]+=volume*env*tone
for bar,chord in enumerate([(50,57,62,66),(47,54,59,62),(43,50,55,59),(45,52,57,61),(43,50,55,59),(45,52,57,64)]):
    for j,idx in enumerate([0,1,2,1,3,1,2,1]):
        voice((bar*8+j)*beat,chord[idx],1.8,1150 if j%2 else 1400)
    for off,note,length in [(1,chord[2]+12,2.6),(5,chord[3]+12,1.8)]:
        voice((bar*8+off)*beat,note,length,480,True)
with wave.open(str(OUT/'willow-theme.wav'),'wb') as f:
    f.setparams((1,2,rate,0,'NONE','not compressed'))
    f.writeframes(array('h',(int(v) for v in mix)).tobytes())

# The upper camp overlooks a sea of cloud. Rock faces use flat, stepped facets.
sky,d=canvas(480,270)
for y in range(270):
    a=min(1,y/175)
    d.line((0,y,480,y),fill=tuple(int(p+(q-p)*a) for p,q in zip((81,120,157),(244,211,178))))
d.ellipse((347,29,371,53),fill='#ffe8c4')
for layer,(base,color,light) in enumerate([(154,'#9ca7b9','#cfced0'),(170,'#748fa3','#ced8d3'),(202,'#556f81','#aebfc3')]):
    for x in range(-60,535,47):
        tip=(x+rng.randrange(8,34),base-rng.randrange(24,80))
        d.polygon([(x-63,base),tip,(x+76,base)],fill=color)
        tx,ty=tip
        d.polygon([tip,(tx+18,ty+17),(tx+8,ty+13),(tx+3,ty+16),(tx-2,ty+8),(tx-14,ty+20)],fill=light)
    for i in range(56):
        x=rng.randrange(-15,480);y=base+rng.randrange(1,10)
        d.ellipse((x,y-3,x+rng.randrange(8,34),y+3),fill='#c6cbd0')
d.polygon([(-5,231),(69,215),(137,218),(190,234),(240,223),(320,189),(390,180),(480,191),(480,270),(0,270)],fill='#8a9599')
d.polygon([(0,228),(69,212),(137,215),(194,231),(243,219),(317,185),(394,176),(480,187),(480,194),(394,183),(318,192),(242,228),(190,240),(137,222),(70,220),(0,240)],fill='#e6e8d7')
d.line((376,179,376,143),fill='#624f48',width=2)
d.polygon([(377,143),(396,146),(388,150),(397,154),(377,153)],fill='#cd7853')
sky.convert('RGB').save(OUT/'summit.png')
icon=Image.new('RGBA',(32,32),'#395c50')
icon.alpha_composite(hero.crop((0,28,24,56)),(4,2))
icon.save(OUT/'kt-icon.png')
