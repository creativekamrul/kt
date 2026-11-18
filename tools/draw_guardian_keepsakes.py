"""Draw the four pocket-journal stamps and synthesize KT's short encounter cues."""
from pathlib import Path
from PIL import Image, ImageDraw
import math
import struct
import wave

OUT = Path(__file__).resolve().parents[1] / "shell" / "assets"
# Each stamp has a different silhouette even when printed in a single ink.
patterns = [
    ("#935739", ["..###...###..",".##.##.##.##.","##...###...##","##.#.###.#.##",".###.###.###.","....#####....","....#.#.#....",".....###.....",".....###....."]),
    ("#675080", [".........####","......#####..","....#####.#..","...####.##...","..###.##.....",".##.###......",".####........","..##.........",".##.........."]),
    ("#b35239", ["....##.##....",".##.##.##.##.",".##.......##.",".....###.....","....#####....","...#######...","..#########..","..#########..","...###.###..."]),
    ("#306f65", [".....###.....","....#...#....","...###.###...","..#..###..#..","..#.##.##.#..","..###...###..",".###########.",".....###.....","......#......"]),
]
sheet = Image.new("RGBA", (32*len(patterns),32))
for i,(ink,rows) in enumerate(patterns):
    stamp = Image.new("RGBA",(32,32))
    pen = ImageDraw.Draw(stamp)
    pen.polygon([(9,1),(22,1),(29,7),(31,16),(28,24),(21,30),(9,30),(2,24),(0,15),(3,7)],
                fill="#f9edcb",outline=ink)
    pen.ellipse((3,3,28,28),outline=ink)
    for y,row in enumerate(rows):
        for x,v in enumerate(row):
            if v=="#": pen.point((9+x,10+y),fill=ink)
    for x in (12,15,18): pen.point((x,24),fill=ink)
    sheet.paste(stamp,(i*32,0))
sheet.save(OUT/"guardian-stamps.png")

# start, duration, starting and ending frequency. Space between notes is part of each cue.
sounds = {
    "copperhorn-hooves": [(0,.14,170,82),(.20,.18,151,72)],
    "moonkite-whistle": [(0,.32,525,870)],
    "ember-paws": [(0,.08,510,360),(.11,.08,560,405),(.22,.11,660,460)],
    "bellback-star": [(0,.50,784,784)],
    "sol-tap": [(0,.09,358,185)],
}
rate=22050
for name,notes in sounds.items():
    mix=[0.] * (1+int(rate*max(start+duration for start,duration,_,_ in notes)))
    for start,duration,f0,f1 in notes:
        for i in range(int(duration*rate)):
            t=i/rate
            phase=2*math.pi*(f0*t+.5*(f1-f0)*t*t/duration)
            decay=1.5 if name=="moonkite-whistle" else 4
            env=min(1,t/.009,(duration-t)/.018)*math.exp(-decay*t/duration)
            overtone=2.4 if name=="bellback-star" else 2
            mix[int(start*rate)+i]+=env*(math.sin(phase)+.2*math.sin(overtone*phase))
    with wave.open(str(OUT/(name+".wav")),"wb") as wav:
        wav.setparams((1,2,rate,0,"NONE","not compressed"))
        wav.writeframes(struct.pack("<"+"h"*len(mix),*(round(5500*v) for v in mix)))
