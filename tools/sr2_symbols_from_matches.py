"""Regenerate port/symbols.csv from port/sr2_sr1_matches.csv (+ the hand-named new functions of S1.5).
Run after sr2index.py, sr2match.py, sr2_modules.py. NOTE: overwrites port/symbols.csv (hand edits
made later must be added to NEW below or merged with tools/merge_symbols.py)."""
import csv, os
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
M = list(csv.DictReader(open(os.path.join(R, 'port/sr2_sr1_matches.csv'))))
mods = {r['sr2_start']: r['module'] for r in csv.DictReader(open(os.path.join(R, 'port/sr2_modules.csv')))}
NEW = """0001:01ea sound_options_menu
0001:8c68 drive_to_event
0001:72f2 car_info_sr2
2c57:05c8 speaker_sound_init
2c57:13d8 adlib_sound_init
2c57:1733 adlib_detect
2c57:16e1 adlib_write
2c57:16c6 adlib_delay_init
2c57:2514 adlib_set_port
2c57:24ad adlib_reset
2c57:26b9 adlib_music_init
2c57:0aa2 snd_bank_and_intro_load
2c57:0ade mus_songs_load
2c57:09ca mus_file_load
2c57:14aa snd_bank_load
2c57:151b snd_bank_init
2c57:2875 mus_tick
2c57:26d9 mus_play
2c57:25aa mus_stop
2c57:25f0 mus_resume
2c57:2542 mus_set_tempo
2c57:252e mus_timer_remove_wrap
2c57:21b1 adlib_vec_off
2c57:21f0 adlib_vec_on
2c57:05ac spk_vec_off
2c57:05ba spk_vec_on
2f1b:0027 pit_set_divisor
2f1b:0045 mus_timer_set_divisor
2f1b:0069 mus_timer_install
2f1b:00a7 mus_timer_remove
2f1b:00bc mus_set_countdown
2f1b:00cb mus_timer_isr
1a21:75ea shp_load_set
1a21:7304 shp_read_block_a
1a21:7446 shp_read_block_b
2925:2714 end_data_load
2925:2e60 video_options_menu2
2925:14b6 tournament_standings
2925:12f2 tournament_round_header
2925:2104 event15_run
2925:1970 event17_run
2925:3174 stickers_menu_sr2"""
rows, named = [], set()
for r in M:
    if r['sr1_name']:
        rows.append(['func', r['sr2_start'], r['sr1_name'], '', r['sr1_module'], '',
                     f"SR1 {r['sr1_start']}, {r['class']} by {r['method']} {r['score']}"])
        named.add(r['sr2_start'])
for l in NEW.split('\n'):
    a, n = l.split()
    if a not in named:
        rows.append(['func', a, n, '', mods[a], '', 'SR2 new, S1.5'])
rows.sort(key=lambda r: (r[1][:4], int(r[1][5:], 16)))
with open(os.path.join(R, 'port/symbols.csv'), 'w', newline='', encoding='utf-8') as o:
    w = csv.writer(o)
    w.writerow('kind,address,name,type,owner,other_names,notes'.split(','))
    w.writerows(rows)
print(len(rows), 'rows')
