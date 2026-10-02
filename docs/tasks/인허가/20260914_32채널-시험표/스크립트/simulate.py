"""Host arithmetic model. DSP library log/power and HEAR FFT are ideal approximations."""
import json
import re
from pathlib import Path
import numpy as np

ROOT = Path.cwd()
OUT = ROOT / 'tmp/sound1-32ch'
FREQS = [3815,100,114,128,2869,3038,3217]
RANGES = [(74,76),(69,71),(119,120),(34,36),(40,42),(98,110)]
VOLS = [1,10]
def src(p): return (ROOT/p).read_text(encoding='utf-8')
def array(s,name):
    m=re.search(r'\b'+re.escape(name)+r'\s*\[[^]]*\]\s*=\s*\{(.*?)\};',s,re.S)
    clean=re.sub(r'//[^\n]*|/\*.*?\*/','',m[1],flags=re.S)
    return [int(x) for x in re.findall(r'-?\d+',clean)]
agc=src('src/1__cfx/signalProcessing/agc.c')
fft=src('src/2__cm3/Gen1_5/FS/ci_fft.c')
half=array(fft,'hanning_window_coeff')
window=np.array(half+half[::-1],dtype=float)/2**23
passbins=array(fft[fft.index('int ci_fft_init_pass_bin_ch_32'):],'pass_bin')
assert len(window)==512 and len(passbins)==256
bandbins=[np.flatnonzero(np.array(passbins)==b) for b in range(32)]
pcb=[31,12,11,9,7,5,3,1,0,2,4,6,8,10,13,15,17,18,20,22,24,26,28,30,14,29,27,25,23,21,19,16]
atten=array(agc,'m_attenuation_region_slope_Q8_16')
ay=array(agc,'m_attenuation_region_ybias_db_Q8_16')
ny=array(agc,'m_noise_region_ybias_db_Q12_12')
amp=array(agc,'m_amplify_region_ybias_db_Q8_16')
loggain=array(src('src/1__cfx/signalProcessing/nonlinearMapping.c'),'g_logarithmMapping_audioVolume_linearGain_Q9_15')
def adc(db): return int(np.floor(262143*10**((db-128.293)/20)))
def xp(x):
    x=np.asarray(x,dtype=np.int64)
    d=np.floor(10*np.log10(np.maximum(x,1)*32/2**47)*65536).astype(np.int64)
    p=(d*1258291)>>23
    z=np.floor(10.0**(p/65536/10)*2**23).astype(np.int64)
    return np.where(x<1,0,(z*5163793)>>23)
def current(rep,vol,e):
    t=211 if e==16 else 0
    xmin=(8*loggain[vol-1])>>15
    a=((211-t)*2**24)//int(xp(21892)-xp(xmin))
    b=211*256-((a*int(xp(21892)))>>16)
    level=np.clip(((a*xp(rep)>>16)+b)>>8,t,211)
    return level,497+((19333*level)>>12)
def simulate(f,db,vol):
    # Exact integer frequencies complete their phase pattern in the final 1 s.
    n=np.arange(48512)
    wave=np.floor(adc(db)*np.sin(2*np.pi*f*n/16000)+1e-9).astype(np.int64).reshape(-1,16)
    peaks=np.maximum(1,np.max(np.abs(wave),axis=1))
    d=np.floor(10*np.log10(peaks*32/2**47)*65536).astype(np.int64)
    target=np.where(d < -7700296,(((131072*d)>>20)+ny[vol-1]-(d>>4))<<4,
                    np.where(d > -6209352,((atten[vol-1]*d)>>16)+ay[vol-1]-d,amp[vol-1]))
    gains=np.empty(len(d),dtype=np.int64)
    prev=0
    for i,t in enumerate(target):
        c,other=(655,64880) if prev<=t else (62259,3276)
        prev=(c*int(t)+other*prev)>>16
        gains[i]=prev
    # Mirror Q1.23 -> Q12.12 truncations around the ideal power function.
    lin=np.floor(10.0**((gains-1972830)/65536/10)*2**23).astype(np.int64)
    lin=(lin*4194304)>>23
    output=np.clip((wave*lin[:,None])>>12,-8388608,8388607).ravel()
    windows=np.lib.stride_tricks.sliding_window_view(output,512)[32000:48000:16]
    # Combined code scaling: abs(DFT)/128; HEAR integer/LUT internal details are not emulated.
    mag=np.floor(np.abs(np.fft.rfft(windows*window,axis=1)[:,:256])/128).astype(np.int64)
    reps=np.column_stack([np.max(mag[:,b],axis=1) for b in bandbins])
    return reps, [int(peaks[-1000:].min()),int(peaks[-1000:].max())]

def main():
    cache={}
    levels=set()
    for lo,hi in RANGES:
        levels.update(range(lo,hi+1)); levels.add((lo+hi)/2)
    # Retain the current ID-6 maximum setting 109~110 as a document vector.
    levels.add(109.5)
    for f in FREQS:
        for vol in VOLS:
            for db in sorted(levels): cache[f,db,vol]=simulate(f,db,vol)
        print('simulated',f,flush=True)
    rows=[]
    cases=[]
    for fi,f in enumerate(FREQS):
        for ri,(lo,hi) in enumerate(RANGES):
            mid=(lo+hi)/2
            for vi,vol in enumerate(VOLS):
                case=fi*12+ri*2+vi+1
                allrep=np.concatenate([cache[f,d,vol][0] for d in sorted(set(range(lo,hi+1))|{mid})])
                nom=cache[f,mid,vol][0]
                low=allrep.min(axis=0); high=allrep.max(axis=0)
                center=np.floor(np.median(nom,axis=0)).astype(int)
                levels32=[]; currents32=[]
                for e in range(1,33):
                    vals=[int(low[e-1]),int(center[e-1]),int(high[e-1])]
                    amps,curs=current(np.array(vals),vol,e)
                    rows.append([case,f,f'{lo}~{hi}',vol,e,*vals])
                    levels32.append(amps.tolist());currents32.append(curs.tolist())
                cases.append(dict(case=case,f=f,lo=lo,hi=hi,mid=mid,vol=vol,adc=[adc(lo),adc(mid),adc(hi)],amps=levels32,currents=currents32))
    data=dict(freqs=FREQS,ranges=[dict(label=f'{lo}~{hi}',lo=lo,hi=hi,mid=(lo+hi)/2,adc=[adc(lo),adc((lo+hi)/2),adc(hi)]) for lo,hi in RANGES],
              rows=rows,cases=cases,pcb=pcb,bandbins=[b.tolist() for b in bandbins],loggain=loggain,
              method='16 kHz, 16-sample integer AGC, 2 s settling, final 1 s / 16-sample hop / 512 Hann FFT. Integer dB grid plus midpoint. Ideal DSP log/power and HEAR magnitude approximation.')
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'model.json').write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(data['ranges'],ensure_ascii=False))
    for case in cases:
        if case['f']==3815 and case['lo'] in (34,69,98):print(case['lo'],case['vol'],case['currents'][25])
if __name__=='__main__':main()
