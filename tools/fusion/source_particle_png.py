"""Lossless original RGB/RGBA PNG crop for native particle sprite sheets."""
import struct,zlib,binascii

def crop_png(source,target,x,y,width,height):
 raw=source.read_bytes()
 if raw[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('source image is not PNG')
 w,h,bits,color,comp,filt,interlace=struct.unpack('>IIBBBBB',raw[16:29])
 if bits!=8 or color not in (2,6) or comp or filt or interlace:raise ValueError('particle PNG decoder requires noninterlaced RGB/RGBA8')
 if min(x,y)<0 or min(width,height)<=0 or x+width>w or y+height>h:raise ValueError('source sprite crop outside texture')
 channels=4 if color==6 else 3;stride=w*channels;data=bytearray();pos=8
 while pos<len(raw):
  n=struct.unpack_from('>I',raw,pos)[0];kind=raw[pos+4:pos+8]
  if kind==b'IDAT':data.extend(raw[pos+8:pos+8+n])
  if kind==b'tRNS':raise ValueError('particle color-key PNG requires decoder')
  pos+=n+12
 decoded=zlib.decompress(data)
 if len(decoded)!=(stride+1)*h:raise ValueError('invalid PNG row length')
 previous=bytearray(stride);out=bytearray()
 def paeth(a,b,c):
  p=a+b-c;da=abs(p-a);db=abs(p-b);dc=abs(p-c)
  return a if da<=db and da<=dc else b if db<=dc else c
 for row_index in range(h):
  filter=decoded[row_index*(stride+1)];row=bytearray(decoded[row_index*(stride+1)+1:(row_index+1)*(stride+1)])
  if filter>4:raise ValueError('invalid PNG filter')
  for k in range(stride):
   a=row[k-channels] if k>=channels else 0;b=previous[k];c=previous[k-channels] if k>=channels else 0
   row[k]=(row[k]+(0,a,b,(a+b)//2,paeth(a,b,c))[filter])&255
  if y<=row_index<y+height:
   out.append(0)
   for col in range(x,x+width):
    out.extend(row[col*channels:col*channels+3]);out.append(row[col*channels+3] if channels==4 else 255)
  previous=row
 def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',binascii.crc32(kind+data)&0xffffffff)
 target.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(out,9))+chunk(b'IEND',b''))
