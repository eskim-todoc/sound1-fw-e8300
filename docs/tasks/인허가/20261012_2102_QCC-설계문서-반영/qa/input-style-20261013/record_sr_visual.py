from pathlib import Path
from datetime import datetime,timezone
import json,hashlib
q=Path(__file__).parent
item=next(d for d in json.loads((q/'authored.json').read_text('utf-8'))['docs'] if d['code']=='SR')
record={'code':'SR','status':'PASS','reviewer':'root','checked_at':datetime.now(timezone.utc).isoformat(),'candidate':item['candidate'],'candidate_sha256':item['candidate_sha256'],'pages':11,'all_pages_opened_full_size':True,'method':'Actual native Word export of saved candidate, pypdfium2 PNG, view_image all pages 1..11','checks':{'cover_history_preserved':True,'TOC_and_page_numbering_1_to_11':True,'heading_number_weight_and_baselines':True,'body10pt_and_memory_item_indentation':True,'no_unexpected_blank_page':True,'no_clipped_text_or_tables':True,'figures_with_captions':True,'QCC_red_content_and_black_unconfirmed_notices_readable':True,'old_E8300_blue_notice_preserved':True},'refinements':['Removed obsolete break before hardware; standards table/hardware/system figure now share page5','Corrected two leading layout tabs in memory explanations','Bold category and verification-question labels; question/English/answer stay together','Continuous page numbers match total pages'], 'page_images':[{'page':i,'path':str(q/'views-final/SR'/f'page-{i:03d}.png'),'sha256':hashlib.sha256((q/'views-final/SR'/f'page-{i:03d}.png').read_bytes()).hexdigest().upper()} for i in range(1,12)]}
assert hashlib.sha256(Path(item['candidate']).read_bytes()).hexdigest().upper()==item['candidate_sha256']
(q/'SR-visual-final-review.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print('SR visual final PASS, 11 pages')
