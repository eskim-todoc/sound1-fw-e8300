"""Use only this bundled Python to render a directory of Office QA PDFs."""
from pathlib import Path
import argparse, subprocess, sys
sys.stdout.reconfigure(encoding='utf-8')
parser=argparse.ArgumentParser()
parser.add_argument('directory')
parser.add_argument('--scale',default='1.5')
args=parser.parse_args()
helper=Path(__file__).with_name('render_pdf.py')
for pdf in sorted(Path(args.directory).glob('*.pdf')):
    subprocess.run([sys.executable,str(helper),str(pdf),'--out',str(pdf.with_suffix('')),'--scale',args.scale,'--contact'],check=True)
