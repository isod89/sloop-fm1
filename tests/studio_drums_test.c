/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint32_t into_s(const track_t *t)          /* samples into the step the clock is in */
{
    uint32_t into, len;
    trk_grid(t, &into, &len);
    return into / (uint32_t)song.g[G_BPM];
}
int main(int argc,char **argv)
{
    uint32_t i,j,k,v,total=0; uint64_t energy[DRUM_KITS]={0};
    int32_t out[CTL*2]; dstep_t step={{0}};
    host_tracks_init();
    for(i=0;i<DRUM_LANES;i++)dstep_set(&step,i,i&3u,(i>>2)&3u);   /* SLOOP 2.0: all 16 lanes in one step */
    assert(dstep_mask(&step)==0xFFFFu);
    for(i=0;i<DRUM_LANES;i++)assert(dstep_has(&step,i) && dstep_lvl(&step,i)==(i&3u) && dstep_rat(&step,i)==((i>>2)&3u));
    dstep_clr(&step,5);assert(!dstep_has(&step,5) && dstep_has(&step,4) && dstep_has(&step,6) && dstep_lvl(&step,6)==2u);
    for(i=0;i<27u;i++)assert(lane_of_key(i)<DRUM_LANES);
    assert(lane_of_key(0)==0 && lane_of_key(1)==0 && lane_of_key(26)==15 && key_of_lane(15)==26);
    for(i=35;i<=81;i++)assert(lane_of_note(i)<DRUM_LANES);
    for(i=0;i<DRUM_LANES;i++)assert(lane_of_note(LANE_NOTE[i])==i);
    /* Render the same hits through all five kits; each must be distinct,
     * finite and silent after its one-shots have finished. */
    for(i=0;i<DRUM_KITS;i++) {
        memset(&drums,0,sizeof drums);drums.set=-2;
        TDRUM->p[P_E0]=(int16_t)i;
        drum_on(36,110);drum_on(38,100);drum_on(46,80);
        for(j=0;j<FS*2u/CTL;j++) {
            int32_t l[CTL]={0},r[CTL]={0},rev[CTL]={0};
            drums_render(l,r,rev,CTL);
            for(k=0;k<CTL;k++){assert(l[k]>-131072 && l[k]<131072);energy[i]+=l[k]<0?-l[k]:l[k];}
        }
        assert(energy[i]>10000);
        for(k=0;k<NDRUM;k++)assert(!drums.v[k].active);
        for(k=0;k<i;k++)assert(energy[k]!=energy[i]);
    }
    /* eight hits at once (long ones: crash, toms, ride) all ring; a ninth takes the oldest */
    {
        static const uint8_t HIT[9]={49,57,41,43,45,47,51,52,39};
        uint32_t ring=0;
        memset(&drums,0,sizeof drums);drums.set=-2;
        TDRUM->p[P_E0]=0;                                 /* the GM sample kit */
        for(i=0;i<8u;i++)drum_on(HIT[i],100);
        for(k=0;k<NDRUM;k++)ring+=drums.v[k].active;
        assert(NDRUM==8 && ring==8);
        drum_on(HIT[8],100);
        for(ring=0,k=0;k<NDRUM;k++)ring+=drums.v[k].active && drums.v[k].note==HIT[0];
        assert(ring==0);                                  /* the crash (the oldest) gave way */
        for(ring=0,k=0;k<NDRUM;k++)ring+=drums.v[k].active;
        assert(ring==8);
        printf("drums: eight hits ring at once, a ninth takes the oldest ok\n");
    }
    /* LIVE metronome (seq.c click_tick): 120 BPM, 2 s = 4 beats; REC mode clicks only while a
     * track records, ON always while playing, OFF never; the first beat of the bar is louder */
    for(v=0;v<4;v++) {
        uint32_t hits=0,loud=0,age0;
        host_tracks_init();memset(&drums,0,sizeof drums);drums.set=-2;
        song.g[G_BPM]=120;song.g[G_CLOCK]=v==3?0:v==2?2:1;song.rec=v==0?1u:0u;rec_wait=0;   /* OFF / ON / REC */
        transport_req=1;age0=drums.age;
        for(j=0;j<(FS*2u-FS/4u)/CTL;j++) {               /* up to just before beat 4 */
            uint32_t a=drums.age;
            mix_block(out,CTL);
            if(drums.age!=a){hits++;for(k=0;k<NDRUM;k++)if(drums.v[k].age==drums.age && drums.v[k].vel>100)loud++;}
        }
        transport_req=2;events_block(CTL);
        if(v==0||v==2)assert(hits==4 && loud==1);
        else assert(hits==0 && drums.age==age0);
    }
    /* LIVE record start. Stopped and armed, a project with notes: the first note starts the
     * loop and is step 1 */
    {
        uint32_t period, b, k0 = 7u, half, c;            /* key 7 = C4 */
        host_tracks_init();song.g[G_BPM]=120;song.g[G_CLOCK]=0;song.sel=0;song.playing=0;
        for(c=0;c<NTRK;c++){memset(trk[c].step,0,sizeof trk[c].step);for(j=0;j<NSTEP;j++)trk[c].step[j].time=ST_REST;}
        for(c=0;c<NTRK;c++)steps_clear(&trk[c]);
        dstep_set(&TDRUM->dstep[3],0,LV_NORM,0);
        rec_wait=1;
        for(b=0;b<10;b++)mix_block(out,CTL);            /* nothing happens while it waits */
        assert(!song.playing && !song.rec && rec_wait==1 && !ft_on);
        fm1_in.notes=1u<<k0;mix_block(out,CTL);
        assert(song.playing && song.rec==1u && !rec_wait && rec_go && !ft_on);
        assert(trk[0].step[0].n==1 && trk[0].step[0].note[0]==60 && trk[0].step[0].time==ST_NOTE);
        fm1_in.notes=0;mix_block(out,CTL);
        transport_req=2;mix_block(out,CTL);
        assert(!song.playing && !song.rec && !rec_wait);     /* STOP ends the take */
        /* playing: recording starts at once; a note goes to the step heard: up to REC_LAT after
         * the middle of a step it is still that step, later the next one */
        song.sel=1;trk[1].p[P_SLEN]=16;period=div_samples((uint32_t)trk[1].p[P_SDIV]);half=period/2u;
        transport_req=1;mix_block(out,CTL);
        while(trk[1].seq_idx!=5u)mix_block(out,CTL);
        rec_begin();assert(song.rec==2u);
        while(into_s(&trk[1])<half+REC_LAT/2u)mix_block(out,CTL);
        fm1_in.notes=1u<<k0;mix_block(out,CTL);fm1_in.notes=0;mix_block(out,CTL);
        assert(trk[1].step[5].n==1 && !trk[1].step[6].n);   /* late by less than the latency: on 5 */
        while(trk[1].seq_idx!=9u)mix_block(out,CTL);
        while(into_s(&trk[1])<half+REC_LAT+2u*CTL)mix_block(out,CTL);
        fm1_in.notes=1u<<(k0+2u);mix_block(out,CTL);fm1_in.notes=0;mix_block(out,CTL);
        assert(!trk[1].step[9].n && trk[1].step[10].n==1 && trk[1].step[10].note[0]==62);
        transport_req=2;mix_block(out,CTL);song.rec=0;rec_wait=0;
    }
    /* FREE TAKE (an empty project): play with no tempo, REC on the downbeat closes the loop,
     * whose length sets the tempo; the notes and their lengths are quantised into it */
    {
        static const struct { double bpm; uint32_t bars; int16_t want_bpm, want_bars; } T[] = {
            {100.0, 1, 100, 1},     /* 1 bar at 100: 100 (200 is further from 120) */
            {118.0, 1, 120, 1},     /* within 3 % of the tempo set: kept */
            {90.0, 2, 90, 2},       /* 2 bars at 90: 45 / 90 / 180, 90 is the nearest 120 */
            {140.0, 4, 140, 4},     /* 4 bars at 140 (35 / 70 / 140) */
        };
        uint32_t ti, c, rb = 1u << 3, k0 = 7u;
        for(ti=0;ti<sizeof T/sizeof T[0];ti++){
            double beat=60.0/T[ti].bpm;
            uint32_t nb=T[ti].bars*4u, blk=0, bi, len=16u*(uint32_t)T[ti].want_bars;
            host_tracks_init();song.g[G_BPM]=120;song.g[G_CLOCK]=0;song.sel=0;song.playing=0;song.rec=0;
            for(c=0;c<NTRK;c++){memset(trk[c].step,0,sizeof trk[c].step);for(j=0;j<NSTEP;j++)trk[c].step[j].time=ST_REST;}
            trk[0].step[40].time=ST_TIE;                 /* (an old tie: no effect) */
            ft_btn_mask=rb;fm1_in.buttons=0;ft_bars=0;
            rec_wait=1;mix_block(out,CTL);
            for(bi=0;bi<=nb;bi++){                       /* a note on every beat, the 2nd held 1/2 beat */
                uint32_t on=(uint32_t)(bi*beat*FS/CTL+0.5)+(bi&1u?6u:0u);   /* (+4 ms late on odd beats) */
                uint32_t off=on+(uint32_t)((bi==1u?beat/2.0:beat/8.0)*FS/CTL);
                if(bi==nb){                              /* the downbeat: REC closes */
                    while(blk<on){mix_block(out,CTL);blk++;}
                    fm1_in.buttons=rb;mix_block(out,CTL);blk++;fm1_in.buttons=0;
                    break;
                }
                while(blk<on){mix_block(out,CTL);blk++;}
                {static const uint8_t KOFF[4]={0,2,4,5};fm1_in.notes=1u<<(k0+KOFF[bi&3u]);}mix_block(out,CTL);blk++;
                if(bi==0)assert(ft_on && !song.playing && !rec_wait);
                while(blk<off){mix_block(out,CTL);blk++;}
                fm1_in.notes=0;mix_block(out,CTL);blk++;
            }
            assert(!ft_on && song.playing && !song.rec);
            assert(ft_bars==(uint8_t)T[ti].want_bars && song.g[G_BPM]==T[ti].want_bpm);
            for(c=0;c<NTRK;c++)assert(trk[c].p[P_SLEN]==(int16_t)len && trk[c].p[P_SDIV]==2);
            for(j=0;j<len;j++){
                const step_t *st=&trk[0].step[j];
                if(j%4u==0){
                    static const uint8_t KN[4]={60,62,64,65};
                    assert(st->time==ST_NOTE && st->n==1 && st->note[0]==KN[(j/4u)&3u]);
                } else if(j==5u) {
                    assert(st->time==ST_TIE);            /* the held note: 2 steps */
                } else {
                    assert(!st->n && st->time!=ST_NOTE && st->time!=ST_TIE);
                }
            }
            for(j=len;j<NSTEP;j++)assert(trk[0].step[j].time==ST_REST);
            transport_req=2;mix_block(out,CTL);
        }
        /* STOP during a free take drops it (so does the PLAY button); a PLAY from elsewhere (the editor)
         * closes it like REC */
        host_tracks_init();song.g[G_BPM]=120;song.playing=0;song.rec=0;song.sel=0;
        for(c=0;c<NTRK;c++){memset(trk[c].step,0,sizeof trk[c].step);for(j=0;j<NSTEP;j++)trk[c].step[j].time=ST_REST;}
        ft_bars=0;rec_wait=1;fm1_in.notes=1u<<k0;mix_block(out,CTL);fm1_in.notes=0;
        assert(ft_on);
        for(j=0;j<100;j++)mix_block(out,CTL);
        transport_req=2;mix_block(out,CTL);
        assert(!ft_on && !song.playing && ft_bars==0xFF && !trk[0].step[0].n);
        ft_bars=0;rec_wait=1;fm1_in.notes=1u<<k0;mix_block(out,CTL);fm1_in.notes=0;
        for(j=0;j<(uint32_t)(2.0*FS/CTL);j++)mix_block(out,CTL);   /* 2 s: 1 bar at 120 */
        transport_req=1;mix_block(out,CTL);
        assert(!ft_on && song.playing && ft_bars==1 && song.g[G_BPM]==120 && trk[0].step[0].n==1);
        transport_req=2;mix_block(out,CTL);              /* the PLAY button during a take: dropped */
        for(c=0;c<NTRK;c++)steps_clear(&trk[c]);
        ft_drop_mask=1u<<5;ft_bars=0;rec_wait=1;fm1_in.notes=1u<<k0;mix_block(out,CTL);fm1_in.notes=0;
        for(j=0;j<200;j++)mix_block(out,CTL);
        fm1_in.buttons=ft_drop_mask;mix_block(out,CTL);fm1_in.buttons=0;mix_block(out,CTL);
        assert(!ft_on && !song.playing && ft_bars==0xFF && !trk[0].step[0].n);
        ft_drop_mask=0;
        /* the longest take closes by itself (24 s) */
        transport_req=2;mix_block(out,CTL);
        for(c=0;c<NTRK;c++){memset(trk[c].step,0,sizeof trk[c].step);for(j=0;j<NSTEP;j++)trk[c].step[j].time=ST_REST;}
        ft_bars=0;rec_wait=1;fm1_in.notes=1u<<k0;mix_block(out,CTL);fm1_in.notes=0;
        while(ft_on)mix_block(out,CTL);
        assert(ft_bars==4 && song.g[G_BPM]==40 && song.playing);
        transport_req=2;mix_block(out,CTL);ft_btn_mask=0;
    }
    /* the arm follows the selected track (rec_follow), and a disarmed song stays disarmed */
    song.rec=1u;rec_follow(2);assert(song.rec==4u);rec_follow(3);assert(song.rec==8u);
    song.rec=0;rec_follow(1);assert(song.rec==0);
    total=(FS*4u/CTL)*CTL;
    FILE *f=argc>1?fopen(argv[1],"wb"):0;
    if(f)wav_hdr(f,total);
    host_tracks_init();song.g[G_BPM]=120;song.rec=1u;transport_req=1;
    for(j=0;j<FS*4u/CTL;j++) {
        mix_block(out,CTL);
        for(k=0;k<CTL;k++)if(f)wav_put(f,out[k*2],out[k*2+1]);
    }
    transport_req=2;events_block(CTL);song.rec=0;
    if(f)fclose(f);
    puts("drums: 16 lanes a step (levels, ratchets), the key / GM maps, every kit distinct, bounded audio and finite tails, metronome (OFF/REC/ON, accent), record: first note starts the loop, at once while playing, latency-compensated steps, free takes set the loop and tempo, arm follows the track PASS");
    return 0;
}
