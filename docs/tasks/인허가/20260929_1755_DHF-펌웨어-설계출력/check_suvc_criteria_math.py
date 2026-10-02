"""Independent host arithmetic, not firmware execution or physical test results."""
from pathlib import Path
import math, json, re
import numpy as np
T=Path(__file__).resolve().parent;ROOT=T.parents[3];Q=T/'qa/suvc-practical-criteria'
checks=[]
def check(name,actual,want):
    assert actual==want,(name,actual,want)
    checks.append({'case':name,'actual':actual,'expected':want})
def sat24(n):return min(8388607,max(-8388608,n))
check('OD 0..15',[sat24(i*64) for i in range(16)],list(range(0,961,64)))
check('OD saturation',[sat24(i*64) for i in [-131073,-131072,0,131071,131072]],[-8388608,-8388608,0,8388544,8388607])
check('single mixer',320//32,10)
check('two input sum',320//32+(-64)//32,8)
check('beamforming boundary',[(64*i//32)//2 for i in range(1,16)]+[(2048//32)//2],list(range(1,16))+[32])
check('gain mix',(10*65536+10*65536)//65536,20)
check('AGC release',(655*65536)//65536,655)
check('AGC attack',(3276*65536)//65536,3276)
check('AGC saturation',[sat24(i*2) for i in [-10,0,10,8388607,-8388608]],[-20,0,20,8388607,-8388608])
ds=[-7700297,-7700296,-7700295,-6209353,-6209352,-6209351]
for gain,nbias,slope,bias,want in [(0,481268,47999,-1661530,[-16,0,0,0,0,51]),(973249,542096,4744,-4786621,[973232,973249,973249,973249,973249,973249])]:
    out=[]
    for d in ds:
        if d < -7700296:out.append(((d//8+nbias-d//16)*16))
        elif d > -6209352:out.append((slope*d)//65536+bias-d)
        else:out.append(gain)
    check('AGC three-region boundaries '+str(gain),out,want)
check('C volume',[50+(100*v)//(2**22) for v in [2936012,3355443,3774873,4194304]],[119,129,139,150])
a=(100*2**24)//65536;b=150*256-a*131072//65536
check('mapping coefficients',[a,b],[25600,-12800])
check('mapping endpoints',[(a*x//65536+b)//256 for x in [65536,131072]],[50,150])
check('frame boundary',[1+(2*p+14)//41 for p in [13,14,25]],[1,2,2])
check('channels per ms',[24//f for f in [1,2,3]],[24,12,8])
check('DAC offset',(100*14435)//32768,44)
check('DAC amplitude',((300-100)*20871)//32768,127)
check('file name return',len('test.app'),8)
for pre,want in [(179999,[180000,False]),(180000,[0,True])]:
    n=pre+1;off=n>180000
    check('sleep boundary '+str(pre),[0 if off else n,off],want)
for amps,want in [([100]*32,list(range(16))),(list(range(32)),list(range(16,32)))]:
    selected=sorted(sorted(range(32),key=lambda i:(amps[i],-i),reverse=True)[:16])
    check('NofM selection',selected,want)
order=[i for pair in zip(range(8),range(8,16)) for i in pair]
check('NofM interleave',order,[0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15])
check('NofM rotation',[order[-1]]+order[:-1],[15,0,8,1,9,2,10,3,11,4,12,5,13,6,14,7])
for bin_index in [32,64]:
    n=np.arange(512);x=np.rint(65536*np.sin(2*np.pi*bin_index*n/512))
    for hann in [np.hanning(512),.5-.5*np.cos(2*np.pi*n/512)]:
        check('FFT peak '+str(bin_index),int(np.argmax(np.abs(np.fft.rfft(x*hann))[:256])),bin_index)
check('FFT zero',float(np.abs(np.fft.rfft(np.zeros(512))).max()),0.0)
# Verify the integer fade endpoint vectors against the actual active table.
src=(ROOT/'src/1__cfx/lib_cfx/lib_i2s.c').read_text('utf-8')
section=src.split('#elif (I2S_BUFFER_UNDERRUN_CNT == 4)')[1].split('#elif')[0]
for direction,want in [('in',[0,15359,49151,65536]),('out',[65536,49151,15359,0])]:
    body=re.search(r'I2S_coeff_fade_'+direction+r'_step\[4\]\[16\]\s*=\s*\{(.*?)\};',section,re.S)[1]
    vals=list(map(int,re.findall(r'\d+',body)))
    check('fade '+direction+' endpoints',[vals[0],vals[15],vals[48],vals[63]],want)
    check('fade '+direction+' count',len(vals),64)
N=3
for prior,want in [(N-1,[N,2]),(N,[N+1,1])]:
    count=prior+1
    check('alt slot boundary '+str(prior),[count,1 if count>N else 2],want)
try:
    from cryptography.hazmat.primitives.ciphers import Cipher,algorithms,modes
    dec=Cipher(algorithms.AES(bytes.fromhex('2b7e151628aed2a6abf7158809cf4f3c')),modes.ECB()).decryptor()
    result=dec.update(bytes.fromhex('3ad77bb40d7a3660a89ecaf32466ef97'))+dec.finalize()
    check('NIST AES ECB public vector',result.hex(),'6bc1bee22e409f96e93d7e117393172a')
except ImportError:
    checks.append({'case':'AES public vector','status':'Host cryptography unavailable; source contract review only'})
(Q/'arithmetic.json').write_text(json.dumps({'scope':'기대값의 독립 호스트 산술/배열 검산. 실제 펌웨어 또는 실기 실행 결과가 아님.','cases':checks},ensure_ascii=False,indent=2),encoding='utf-8')
print('Independent expectation checks:',len(checks))
