#!/usr/bin/env python3
"""Art for canvas-kart into src/assets/kart/ (Mario-style fan art for a personal handheld):
  kart.png   player go-kart as a 5-frame steering SHEET (rear view; red-cap plumber + fat rear wheels; leans)
  karttop.png top-down rival kart (white body + head, tinted per-racer at runtime)
  road.png   bright day asphalt + white edges + dashed center line
  grass.png  bright day grass
  boost.png  a boost-pad strip: cyan chevrons pointing forward (tiled along a road quad)
Run: python3 tools/gen_kart_assets.py
"""
import os
from PIL import Image, ImageDraw, ImageFont
HERE=os.path.dirname(os.path.abspath(__file__)); OUT=os.path.join(HERE,"..","src","assets","kart")
os.makedirs(OUT,exist_ok=True); T=128
def save(im,n): im.save(os.path.join(OUT,n)); print("wrote",os.path.join("assets/kart",n),im.size)

def road():
    import random; rnd=random.Random(7)
    im=Image.new("RGBA",(T,T),(96,98,104,255)); px=im.load()
    for y in range(T):                                                        # speckled asphalt aggregate
        for x in range(T):
            v=104+rnd.randint(-9,9)
            if rnd.random()<0.05: v-=24
            px[x,y]=(max(0,v-8),max(0,v-6),max(0,v+2),255)
    d=ImageDraw.Draw(im)
    d.rectangle([0,0,7,T-1],fill=(228,228,234,255)); d.rectangle([T-8,0,T-1,T-1],fill=(228,228,234,255))  # edge lines
    for y in range(T):
        if (y//24)%2==0: d.rectangle([T//2-4,y,T//2+3,y],fill=(234,236,242,255))  # dashed centre
    return im
def grass():
    import random; rnd=random.Random(11)
    im=Image.new("RGBA",(T,T),(74,158,72,255)); px=im.load()
    for y in range(T):                                                        # mottled turf
        for x in range(T):
            n=rnd.randint(-13,13); px[x,y]=(max(0,58+n),max(0,150+n),max(0,56+n),255)
    d=ImageDraw.Draw(im)
    for _ in range(260):                                                      # scattered blades/tufts
        x=rnd.randint(0,T-1); y=rnd.randint(2,T-1)
        d.line([(x,y),(x,y-2)], fill=((46,126,44,255) if rnd.random()<0.5 else (98,198,94,255)))
    return im
def boost():
    im=Image.new("RGBA",(T,T),(245,185,35,255)); d=ImageDraw.Draw(im)     # gold boost pad
    d.rectangle([0,0,9,T-1],fill=(40,90,175,255)); d.rectangle([T-10,0,T-1,T-1],fill=(40,90,175,255))  # blue side rails
    for base in range(0,T*2,42):                           # forward-pointing chevrons (v = along track)
        for x in range(0,T,2):
            yy=base-abs(x-T//2)
            for w in range(0,11):
                if 0<=yy+w<T: d.point((x,yy+w),fill=(255,250,205,255))
    return im
def kart_sheet():
    FW,FH,N,SS=128,96,5,2                                  # draw at 2x, downscale for anti-aliased edges
    sh=Image.new("RGBA",(FW*N,FH),(0,0,0,0))
    outline=(24,20,28,255)
    body,body_hi=(226,42,38,255),(255,112,104,255)         # red kart body
    frame=(40,84,196,255)                                  # blue frame / rear
    chrome=(198,202,212,255)
    tire=(28,28,32,255)
    rimY,hubB,cap_c=(250,192,40,255),(44,96,206,255),(232,232,238,255)   # yellow rim, blue hub, center cap
    stripe=(238,240,245,255)                               # white centre stripe
    skin,skin_lo=(255,199,151,255),(226,164,116,255)
    hair=(78,46,26,255)                                    # brown hair + mustache
    cap,cap_hi=(226,42,38,255),(252,96,90,255)             # red cap
    shirt,shirt_hi=(220,44,40,255),(250,96,90,255)         # red shirt / sleeves
    ovr,ovr_hi=(40,80,192,255),(86,126,236,255)            # blue overalls
    glove=(248,248,250,255); button=(246,208,70,255)       # white gloves, gold buttons
    for i in range(N):
        dx=(i-2)*10                                        # steering lean (2x space)
        W2,H2=FW*SS,FH*SS; cx=W2//2
        im=Image.new("RGBA",(W2,H2),(0,0,0,0)); d=ImageDraw.Draw(im)
        d.ellipse([cx-152,H2-38,cx+152,H2-6],fill=(0,0,0,90))              # baked soft shadow
        # ---- chunky off-road rear tires: black tire, yellow rim, blue hub ----
        for sx in (-1,1):
            x=cx+sx*106
            d.ellipse([x-46,H2-98,x+46,H2-10],fill=tire,outline=outline,width=5)
            d.ellipse([x-41,H2-92,x+41,H2-16],outline=(58,58,64,255),width=3)     # tread ring
            d.ellipse([x-27,H2-78,x+27,H2-30],fill=rimY,outline=outline,width=4)  # yellow rim
            d.ellipse([x-14,H2-66,x+14,H2-42],fill=hubB,outline=outline,width=3)  # blue hub
            d.ellipse([x-5,H2-58,x+5,H2-50],fill=cap_c)                           # center cap
        # ---- chassis: red with white centre stripe + blue rear frame ----
        d.rectangle([cx-90,H2-96,cx+90,H2-42],fill=body,outline=outline,width=4)
        d.rectangle([cx-84,H2-96,cx+84,H2-78],fill=body_hi)                       # top highlight
        d.rectangle([cx-11,H2-96,cx+11,H2-42],fill=stripe)                        # white centre stripe
        d.rectangle([cx-104,H2-52,cx+104,H2-26],fill=frame,outline=outline,width=4)  # blue rear frame
        d.ellipse([cx-17,H2-54,cx+17,H2-24],fill=(245,245,245,255),outline=outline,width=4)  # number plate
        d.ellipse([cx-8,H2-46,cx+8,H2-32],fill=body)
        # ---- chrome exhausts + blue coil hint ----
        for sx in (-1,1):
            x=cx+sx*30; d.rectangle([x-7,H2-118,x+7,H2-94],fill=chrome,outline=outline,width=3)
        for cyk in range(H2-116,H2-94,6):
            d.arc([cx-15,cyk,cx+15,cyk+9],10,350,fill=(70,120,224,255),width=3)   # spring coils
        # ---- driver: red-cap plumber, rear view ----
        d.rectangle([cx-48+dx,H2-150,cx+48+dx,H2-96],fill=shirt,outline=outline,width=4) # red shirt + sleeves
        d.rectangle([cx-42+dx,H2-150,cx+42+dx,H2-134],fill=shirt_hi)                      # shoulder highlight
        for sx in (-1,1):                                                  # white gloves gripping the wheel
            x=cx+sx*48+dx; d.ellipse([x-16,H2-120,x+16,H2-88],fill=glove,outline=outline,width=4)
        d.rectangle([cx-40+dx,H2-134,cx+40+dx,H2-92],fill=ovr,outline=outline,width=4)  # blue overalls (bib)
        d.rectangle([cx-34+dx,H2-134,cx+34+dx,H2-118],fill=ovr_hi)
        for sx in (-1,1):                                                  # shoulder straps + gold buttons
            x=cx+sx*24+dx; d.rectangle([x-9,H2-150,x+9,H2-122],fill=ovr,outline=outline,width=3)
            d.ellipse([x-6,H2-128,x+6,H2-116],fill=button,outline=outline,width=2)
        d.rectangle([cx-13+dx,H2-158,cx+13+dx,H2-148],fill=skin_lo)                     # neck
        d.ellipse([cx-40+dx,H2-188,cx+40+dx,H2-148],fill=skin,outline=outline,width=4)  # head (rear)
        d.chord([cx-40+dx,H2-172,cx+40+dx,H2-148],15,165,fill=hair)                     # hair at the nape
        d.ellipse([cx-54+dx,H2-162,cx-30+dx,H2-146],fill=hair,outline=outline,width=2)  # mustache/sideburn (L)
        d.ellipse([cx+30+dx,H2-162,cx+54+dx,H2-146],fill=hair,outline=outline,width=2)  # mustache/sideburn (R)
        d.pieslice([cx-44+dx,H2-194,cx+44+dx,H2-148],180,360,fill=cap,outline=outline,width=4)  # red cap dome
        d.chord([cx-44+dx,H2-162,cx+44+dx,H2-150],182,358,fill=cap)                     # cap band across the back
        d.pieslice([cx-30+dx,H2-188,cx+30+dx,H2-158],200,340,fill=cap_hi)              # cap highlight
        sh.paste(im.resize((FW,FH),Image.LANCZOS),(i*FW,0))
    return sh
save(road(),"road.png"); save(grass(),"grass.png"); save(boost(),"boost.png"); save(kart_sheet(),"kart.png")

def ramp():
    im=Image.new("RGBA",(T,T),(230,140,30,255)); d=ImageDraw.Draw(im)
    for base in range(0,T*2,40):
        for x in range(0,T,2):
            yy=base-abs(x-T//2)
            for w in range(0,12):
                if 0<=yy+w<T: d.point((x,yy+w),fill=(255,240,180,255))
    return im
def kart_top():
    S=80; im=Image.new("RGBA",(S*2,S*2),(0,0,0,0)); d=ImageDraw.Draw(im); c=S  # top-down, nose = up (-y); 2x
    ol=(24,20,28,255)
    d.rectangle([c-30,c-46,c-18,c-6],fill=(30,30,36,255),outline=ol,width=3)   # wheels (dark, fixed)
    d.rectangle([c+18,c-46,c+30,c-6],fill=(30,30,36,255),outline=ol,width=3)
    d.rectangle([c-34,c+6,c-20,c+50],fill=(30,30,36,255),outline=ol,width=3)
    d.rectangle([c+20,c+6,c+34,c+50],fill=(30,30,36,255),outline=ol,width=3)
    d.rectangle([c-26,c-40,c+26,c+50],fill=(255,255,255,255),outline=ol,width=4) # body (WHITE -> per-rival tint)
    d.polygon([(c-20,c-40),(c+20,c-40),(c+12,c-58),(c-12,c-58)],fill=(255,255,255,255),outline=ol) # nose
    d.rectangle([c-16,c-56,c+16,c-46],fill=(210,210,220,255))                    # front bumper (light)
    for sx in (-1,1):                                                            # white gloves on the wheel
        d.ellipse([c+sx*20-7,c-6,c+sx*20+7,c+8],fill=(240,240,245,255),outline=ol,width=2)
    d.ellipse([c-16,c-13,c+16,c+17],fill=(120,120,130,255),outline=ol,width=3)   # head (grey -> tinted)
    d.rectangle([c-11,c-24,c+11,c-11],fill=(235,235,240,255),outline=ol,width=2) # cap brim (forward, light)
    return im.resize((S,S),Image.LANCZOS)
save(ramp(),"ramp.png"); save(kart_top(),"karttop.png")

def hills():
    import math
    Wd,Hh=1600,150; im=Image.new("RGBA",(Wd,Hh),(0,0,0,0)); d=ImageDraw.Draw(im)
    for col,amp,base,freq,ph in [((150,185,160),22,52,0.009,0.0),((104,162,104),40,34,0.0055,2.0)]:
        for x in range(0,Wd+1,3):
            y=int(base+amp*(0.5+0.5*math.sin(x*freq+ph))+9*math.sin(x*freq*3.3+ph))
            d.rectangle([x-3,Hh-y,x+3,Hh],fill=col+(255,))
    return im
save(hills(),"hills.png")

CHARS=[  # name, body, helmet, face, ears
 ("RUSTY",(228,64,64),(245,225,70),(238,186,128),"pointy"),
 ("HOPPER",(86,196,104),(240,240,245),(120,210,120),"wide"),
 ("WHISKERS",(150,155,165),(70,130,220),(205,205,210),"triangle"),
 ("BRUNO",(150,100,60),(220,70,60),(182,132,92),"round"),
 ("VOLT",(80,200,220),(185,192,205),(205,214,224),"square"),
 ("SHELLY",(70,182,160),(96,182,96),(150,205,150),"round"),
]
def _ears(d,cx,yt,face,ear):
    fc=face+(255,); ol=(24,20,28,255)
    for sgn in (-1,1):
        x=cx+sgn*32
        if ear=="pointy": d.polygon([(x-8,yt+6),(x+8,yt+6),(x,yt-34)],fill=fc,outline=ol)
        elif ear=="wide": d.ellipse([x-20,yt-10,x+20,yt+16],fill=fc,outline=ol)
        elif ear=="triangle": d.polygon([(x-10,yt+4),(x+10,yt+4),(x,yt-26)],fill=fc,outline=ol)
        elif ear=="round": d.ellipse([x-16,yt-18,x+16,yt+8],fill=fc,outline=ol)
        elif ear=="square": d.rectangle([x-15,yt-18,x+15,yt+8],fill=fc,outline=ol)
def kart_char_sheet():
    FW,FH,NC,NF,SS=128,96,len(CHARS),5,2
    sh=Image.new("RGBA",(FW*NF,FH*NC),(0,0,0,0))
    for r,(nm,body,helm,face,ear) in enumerate(CHARS):
        jkt=tuple(int(v*0.7) for v in body); bhi=tuple(min(255,int(v*1.25)) for v in body); blo=tuple(int(v*0.68) for v in body)
        for cflag in range(NF):
            dx=(cflag-2)*10
            W2,H2=FW*SS,FH*SS; cx=W2//2
            im=Image.new("RGBA",(W2,H2),(0,0,0,0)); d=ImageDraw.Draw(im); ol=(24,20,28,255)
            d.ellipse([cx-150,H2-40,cx+150,H2-6],fill=(0,0,0,95))
            for sx in (-1,1):
                x=cx+sx*102; d.ellipse([x-40,H2-92,x+40,H2-14],fill=(30,30,34,255),outline=ol,width=4); d.ellipse([x-24,H2-78,x+24,H2-40],fill=(120,122,132,255))
            d.rectangle([cx-92,H2-96,cx+92,H2-44],fill=body+(255,),outline=ol,width=4)
            d.rectangle([cx-86,H2-96,cx+86,H2-76],fill=bhi+(255,))
            d.rectangle([cx-104,H2-56,cx+104,H2-30],fill=blo+(255,),outline=ol,width=4)
            d.ellipse([cx-18,H2-56,cx+18,H2-24],fill=(245,245,245,255),outline=ol,width=4); d.ellipse([cx-9,H2-48,cx+9,H2-32],fill=body+(255,))
            for sx in (-1,1):
                x=cx+sx*70; d.rectangle([x-9,H2-120,x+9,H2-92],fill=(120,122,132,255),outline=ol,width=3)
            d.rectangle([cx-40+dx,H2-150,cx+40+dx,H2-92],fill=jkt+(255,),outline=ol,width=4)
            _ears(d,cx+dx,H2-168,face,ear)
            d.ellipse([cx-40+dx,H2-186,cx+40+dx,H2-116],fill=face+(255,),outline=ol,width=4)
            d.ellipse([cx-10+dx,H2-138,cx+10+dx,H2-120],fill=tuple(int(v*0.85) for v in face)+(255,))
            d.pieslice([cx-42+dx,H2-190,cx+42+dx,H2-140],180,360,fill=helm+(255,),outline=ol,width=4)
            d.rectangle([cx-34+dx,H2-152,cx+34+dx,H2-138],fill=(40,38,46,255))
            d.ellipse([cx-30+dx,H2-154,cx-8+dx,H2-136],fill=(150,205,240,255),outline=ol,width=2)
            d.ellipse([cx+8+dx,H2-154,cx+30+dx,H2-136],fill=(150,205,240,255),outline=ol,width=2)
            sh.paste(im.resize((FW,FH),Image.LANCZOS),(cflag*FW,r*FH))
    return sh
save(kart_char_sheet(),"karts.png")

def tree():
    W2,H2=192,256; im=Image.new("RGBA",(W2,H2),(0,0,0,0)); d=ImageDraw.Draw(im); ol=(24,40,20,255)
    d.rectangle([W2//2-16,H2-70,W2//2+16,H2-6],fill=(110,74,44,255),outline=(60,40,24,255),width=4)  # trunk
    for (cx,cy,rr,col) in [(W2//2,120,92,(60,150,64)),(W2//2-48,150,64,(72,168,74)),(W2//2+48,150,64,(72,168,74)),(W2//2,70,70,(90,188,92))]:
        d.ellipse([cx-rr,cy-rr,cx+rr,cy+rr],fill=col+(255,),outline=ol,width=3)
    return im.resize((96,128),Image.LANCZOS)
def banner():
    W2,H2=512,80; im=Image.new("RGBA",(W2,H2),(0,0,0,0)); d=ImageDraw.Draw(im)
    d.rectangle([0,0,W2-1,H2-1],fill=(235,235,240,255))
    for y in range(0,H2,20):
        for x in range(0,W2,20):
            if ((x//20)+(y//20))%2==0: d.rectangle([x,y,x+19,y+19],fill=(30,30,34,255))
    d.rectangle([0,0,W2-1,6],fill=(220,60,60,255)); d.rectangle([0,H2-7,W2-1,H2-1],fill=(220,60,60,255))
    return im.resize((256,40),Image.LANCZOS)
save(tree(),"tree.png"); save(banner(),"banner.png")

def wall():
    im=Image.new("RGBA",(T,T),(216,58,52,255)); d=ImageDraw.Draw(im)
    for x in range(0,T,64): d.rectangle([x+32,0,x+63,T-1],fill=(240,240,245,255))
    d.rectangle([0,0,T-1,8],fill=(70,72,80,255)); d.rectangle([0,T-9,T-1,T-1],fill=(70,72,80,255))
    return im
save(wall(),"wall.png")

# item box: a wooden crate face — planks + a bright "?" (drawn on all 6 faces at runtime)
FONT=os.path.join(HERE,"..","src","assets","common","ui.ttf")
def boxwood():
    im=Image.new("RGBA",(T,T),(156,104,56,255)); d=ImageDraw.Draw(im)
    for y in range(0,T,32): d.rectangle([0,y,T-1,y+3],fill=(112,72,38,255))       # plank seams
    for x in range(8,T,26): d.line([(x,0),(x,T)],fill=(140,92,50,255))            # grain
    d.rectangle([0,0,T-1,T-1],outline=(86,56,30,255),width=5)                      # crate frame
    try: font=ImageFont.truetype(FONT,92)
    except Exception: font=None
    if font:
        bb=d.textbbox((0,0),"?",font=font); w=bb[2]-bb[0]; h=bb[3]-bb[1]
        px=(T-w)//2-bb[0]; py=(T-h)//2-bb[1]
        for dx,dy in [(-3,-3),(3,-3),(-3,3),(3,3),(0,4)]: d.text((px+dx,py+dy),"?",font=font,fill=(45,30,12,255))
        d.text((px,py),"?",font=font,fill=(255,214,64,255))
    return im
save(boxwood(),"box.png")

def shell_tex():
    im=Image.new("RGBA",(T,T),(58,58,62,255)); d=ImageDraw.Draw(im)   # dark seams
    cell=30
    for ri,cy in enumerate(range(-cell, T+cell, cell)):
        off=(cell//2) if (ri%2) else 0
        for cx in range(-cell+off, T+cell, cell):
            d.ellipse([cx+3,cy+3,cx+cell-3,cy+cell-3], fill=(234,231,224,255))   # light scute
            d.ellipse([cx+8,cy+7,cx+cell-9,cy+cell-12], fill=(248,246,240,255))  # scute highlight
    save(im,"shell_tex.png")
shell_tex()

def kart_tex():                       # subtle metal-panel detail (light, edge AO, rivets, seam) — multiplied over material colour
    import random; rnd=random.Random(5)
    im=Image.new("RGBA",(T,T),(240,240,244,255)); d=ImageDraw.Draw(im)
    for i in range(11): v=240-(11-i)*7; d.rectangle([i,i,T-1-i,T-1-i],outline=(v,v,min(255,v+2),255))   # edge AO -> panel gaps
    px=im.load()
    for _ in range(600):
        x=rnd.randint(0,T-1); y=rnd.randint(0,T-1); c=rnd.randint(-9,4)
        r,g,b,a=px[x,y]; px[x,y]=(max(0,r+c),max(0,g+c),max(0,b+c),255)                                 # fine noise
    for rx,ry in [(15,15),(T-15,15),(15,T-15),(T-15,T-15)]: d.ellipse([rx-3,ry-3,rx+3,ry+3],fill=(158,158,164,255))  # rivets
    d.line([(0,T//2),(T-1,T//2)],fill=(206,206,212,255))                                                # panel seam
    return im
save(kart_tex(),"kart_tex.png")

def rock_tex():                       # mottled stone (light-ish base so material colour shows) + cracks
    import random; rnd=random.Random(3)
    im=Image.new("RGBA",(T,T),(208,205,200,255)); px=im.load()
    for y in range(T):
        for x in range(T):
            n=rnd.randint(-24,14); r,g,b,a=px[x,y]; px[x,y]=(max(0,r+n),max(0,g+n),max(0,b+n-2),255)
    d=ImageDraw.Draw(im)
    for _ in range(10):
        x0=rnd.randint(0,T); y0=rnd.randint(0,T); d.line([(x0,y0),(x0+rnd.randint(-42,42),y0+rnd.randint(-42,42))],fill=(120,116,110,255))
    save(im,"rock_tex.png")
rock_tex()

def bark_tex():                       # brown vertical grain
    import random; rnd=random.Random(4)
    im=Image.new("RGBA",(T,T),(120,80,46,255)); d=ImageDraw.Draw(im)
    for x in range(0,T,3):
        c=rnd.randint(-26,18); d.line([(x,0),(x,T)],fill=(max(0,120+c),max(0,80+c),max(0,46+c),255))
    for _ in range(45): d.point((rnd.randint(0,T-1),rnd.randint(0,T-1)),fill=(88,56,30,255))
    save(im,"bark_tex.png")
bark_tex()

def leaf_tex():                       # mottled foliage green + leaf clusters/gaps
    import random; rnd=random.Random(6)
    im=Image.new("RGBA",(T,T),(56,138,54,255)); px=im.load()
    for y in range(T):
        for x in range(T):
            n=rnd.randint(-18,28); px[x,y]=(max(0,44+n),max(0,124+n),max(0,42+n),255)
    d=ImageDraw.Draw(im)
    for _ in range(80):
        x=rnd.randint(0,T-1); y=rnd.randint(0,T-1)
        d.ellipse([x-2,y-2,x+2,y+2],fill=((98,198,86,255) if rnd.random()<0.6 else (32,94,34,255)))
    save(im,"leaf_tex.png")
leaf_tex()

def rock_ground():                    # mountain ground: rough gray rock with darker fissures
    import random; rnd=random.Random(21)
    im=Image.new("RGBA",(T,T),(120,118,122,255)); px=im.load()
    for y in range(T):
        for x in range(T):
            n=rnd.randint(-20,20); v=(rnd.random()<0.08)
            px[x,y]=(max(0,112+n-(30 if v else 0)),max(0,110+n-(30 if v else 0)),max(0,116+n-(28 if v else 0)),255)
    d=ImageDraw.Draw(im)
    for _ in range(14):                                                  # cracks/fissures
        x0=rnd.randint(0,T); y0=rnd.randint(0,T); d.line([(x0,y0),(x0+rnd.randint(-50,50),y0+rnd.randint(-50,50))],fill=(70,68,72,255))
    save(im,"rock_ground.png")
rock_ground()

def sand():                           # beach sand: warm tan, faint ripples
    import random; rnd=random.Random(22)
    im=Image.new("RGBA",(T,T),(224,206,158,255)); px=im.load()
    for y in range(T):
        for x in range(T):
            n=rnd.randint(-12,12); px[x,y]=(max(0,224+n),max(0,206+n),max(0,150+n),255)
    d=ImageDraw.Draw(im)
    for y in range(0,T,6):                                              # gentle ripple lines
        d.line([(0,y+rnd.randint(-2,2)),(T,y+rnd.randint(-2,2))],fill=(212,192,142,255))
    for _ in range(40): d.point((rnd.randint(0,T-1),rnd.randint(0,T-1)),fill=(200,180,132,255))
    save(im,"sand.png")
sand()

def water():                          # flat sea/river: blue with lighter ripple streaks
    import random; rnd=random.Random(23)
    im=Image.new("RGBA",(T,T),(46,110,168,255)); px=im.load()
    for y in range(T):
        for x in range(T):
            n=rnd.randint(-10,10); px[x,y]=(max(0,40+n),max(0,104+n),max(0,164+n),255)
    d=ImageDraw.Draw(im)
    for _ in range(70):                                                 # ripple highlights
        x=rnd.randint(0,T-1); y=rnd.randint(0,T-1); w=rnd.randint(3,9)
        d.line([(x,y),(x+w,y)],fill=(120,180,220,255))
    save(im,"water.png")
water()
