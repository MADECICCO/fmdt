/**
 * Copyright (c) 2021-2022, Clara CIOCAN, LIP6 Sorbonne University
 * Copyright (c) 2021-2022, Mathuran KANDEEPAN, LIP6 Sorbonne University
 */

#include "ffmpeg-io/reader.h"
#include "ffmpeg-io/writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#include "nrutil.h"
#include "Args.h"
#include "Video.h"
#include "CCL.h"
#include "Features.h"
#include "Threshold.h"
#include "DebugUtil.h"
#include "macro_debug.h"
#include "Tracking.h"
#include "Ballon.h"

#define SIZE_BUF 10000
#define INF 9999999


static Buf buffer[SIZE_BUF];
elemBB* tabBB[NB_FRAMES];

extern uint32 **nearest;
extern float32 **distances;
extern uint32 *conflicts;
extern char path_bounding_box[200];

unsigned track_count_objects(const Track* tracks, const int n_tracks, unsigned *n_stars, unsigned *n_meteors, unsigned *n_noise)
{
    (*n_stars) = (*n_meteors) = (*n_noise) = 0;
    for (int i = 0; i < n_tracks; i++)
        if (tracks[i].time)
            switch (tracks[i].obj_type) {
                case STAR:   (*n_stars  )++; break;
                case METEOR: (*n_meteors)++; break;
                case NOISE:  (*n_noise  )++; break;
                default:
                    fprintf(stderr, "(EE) This should never happen ('tracks[i].obj_type = %d', 'i = %d')\n", tracks[i].obj_type, i);
                    exit(1);
            }

    return (*n_stars) + (*n_meteors) + (*n_noise);
}

void initTabBB()
{
    for(int i = 0; i < NB_FRAMES; i++){
        tabBB[i] = NULL;
    }
}

void addToList(uint16 rx, uint16 ry, uint16 bb_x, uint16 bb_y, uint16 track_id, int frame)
{
    assert(frame < NB_FRAMES);
    elemBB *newE = (elemBB*)malloc(sizeof(elemBB));
    newE->rx = rx;
    newE->ry = ry;
    newE->bb_x = bb_x;
    newE->bb_y = bb_y;
    newE->track_id = track_id;
    newE->next = tabBB[frame];
    tabBB[frame] = newE;
}

void clear_index_Track(Track *tracks, int i)
{
    memset(&tracks[i], 0, sizeof(Track));
}

void init_Track(Track *tracks, int n)
{
    for (int i = 0; i < n ; i++)
        clear_index_Track(tracks, i);
}

void clear_index_buffer(int i)
{
    MeteorROI *s0 = &buffer[i].stats0; memset(s0, 0, sizeof(MeteorROI));
    MeteorROI *s1 = &buffer[i].stats1; memset(s1, 0, sizeof(MeteorROI));
    buffer[i].frame = 0;
}

void clear_buffer_history(int frame, int history_size){
    int diff;
    for(int i = 0; i < SIZE_BUF; i++){
        if(buffer[i].frame != 0){
            diff = frame - buffer[i].frame;
            if (diff >= history_size)
                clear_index_buffer(i);
        }
    }  
}

void insert_buffer(MeteorROI stats0, MeteorROI stats1, int frame)
{
    for (int i = 0; i < SIZE_BUF; i++)
        if (buffer[i].stats0.ID == 0) {
            memcpy(&buffer[i].stats0, &stats0, sizeof(MeteorROI));
            memcpy(&buffer[i].stats1, &stats1, sizeof(MeteorROI));
            buffer[i].frame = frame;
            return;
        }
    fprintf(stderr, "(EE) This sould never happen, out of buffer ('SIZE_BUF' = %d)\n", SIZE_BUF);
    exit(-1);
}

int search_buf_stat(int ROI_id, int frame)
{
    for (int i = 0; i < SIZE_BUF; i++)
        if (frame  == buffer[i].frame + 1 && ROI_id == buffer[i].stats0.ID)
            return i;
    return -1;
}

void Track_extrapolate(Track *t, int theta, int tx, int ty)
{   
    float32 u,v;
    float32 x,y;
    t->state = TRACK_LOST;
    // compensation du mouvement + calcul vitesse entre t-1 et t
    u = t->end.x-t->end.dx - t->x; 
    v = t->end.y-t->end.dy - t->y; 

    x = tx + t->end.x * cos(theta) - t->end.y * sin(theta);
    y = ty + t->end.x * sin(theta) + t->end.y * cos(theta);

    t->x = x + u;
    t->y = y + v;    

    fdisp(t->x);
    fdisp(t->y);
}

void update_bounding_box(Track* track, MeteorROI stats, int frame)
{
    PUTS("UPDATE BB");
    idisp(stats.xmin);
    idisp(stats.xmax);
    uint16 rx, ry, bb_x, bb_y;

    assert(stats.xmin || stats.xmax || stats.ymin || stats.ymax);

    bb_x = (uint16)ceil((double)((stats.xmin + stats.xmax))/2);
    bb_y = (uint16)ceil((double)((stats.ymin + stats.ymax))/2);
    rx   = (bb_x - stats.xmin) +5;
    ry   = (bb_y - stats.ymin) +5;

    // juste pour debug (affichage)
    track->bb_x = bb_x;
    track->bb_y = bb_y;
    track->rx   = rx;
    track->ry   = ry;

    addToList(rx, ry, bb_x, bb_y, track->id, frame-1);
}

void updateTrack(Track *tracks, MeteorROI *stats0, MeteorROI *stats1, int nc1, int frame, int *offset, int *last, int theta, int tx, int ty, int r_extrapol, int d_line, int track_all)
{
    int next;
    int i;

    for (i = *offset; i <= *last; i++){
        next = tracks[i].end.next;
        if(!next){
            *offset = i;
            break; 
        }
    }
    for (i = *offset; i <= *last; i++){
        if(tracks[i].time > 150 && !track_all){
            clear_index_Track(tracks, i);
            continue;
        }
        if(tracks[i].time && tracks[i].state != TRACK_FINISHED ){

            if(tracks[i].state == TRACK_EXTRAPOLATED){
                PUTS("TRACK_EXTRAPOLATED");
                for(int j = 1; j <= nc1; j++){
                        if ( (stats0[j].x > tracks[i].x - r_extrapol) && (stats0[j].x < tracks[i].x + r_extrapol) && (stats0[j].y < tracks[i].y + r_extrapol) && (stats0[j].y > tracks[i].y - r_extrapol)){
                            idisp(stats1[j].ID);
                            tracks[i].end = stats0[j];
                            stats0[j].track_id = tracks[i].id;
                            tracks[i].state = TRACK_UPDATED;
                            update_bounding_box(tracks+i, stats0[j], frame-1);
                        }
                }
            }

            if(tracks[i].state == TRACK_LOST){
                PUTS("TRACK_LOST");
                for(int j = 1; j <= nc1; j++){
                    if (!stats1[j].prev){
                        if ( (stats1[j].x > tracks[i].x - r_extrapol) && (stats1[j].x < tracks[i].x + r_extrapol) && (stats1[j].y < tracks[i].y + r_extrapol) && (stats1[j].y > tracks[i].y - r_extrapol)){
                            tracks[i].state = TRACK_EXTRAPOLATED;
                            tracks[i].time+=2;
                            stats1[j].state = 1;
                        }
                    }
                }
                if (tracks[i].state != TRACK_EXTRAPOLATED){
                    PUTS("FINISHED");
                    tracks[i].state = TRACK_FINISHED;
                }
        
            }
            if(tracks[i].state == TRACK_UPDATED || tracks[i].state == TRACK_NEW){
                next = stats0[tracks[i].end.ID].next;
                if(next){
                    float32 a;
                    float32 dx = (stats1[next].x - stats0[tracks[i].end.ID].x);
                    float32 dy = (stats1[next].y - stats0[tracks[i].end.ID].y) ;

                    a = (dx == 0) ? INF : (dy/dx);

                    float32 y = tracks[i].a * stats1[next].x + tracks[i].b;

                    if ( (fabs(stats1[next].y - y) < d_line) &&
                         ((dx*tracks[i].dx >= 0.0f) && (dy*tracks[i].dy >= 0.0f)) &&
                         ((a < 0 && tracks[i].a < 0) || (a > 0 && tracks[i].a > 0) || ((a == INF) && (tracks[i].a == INF))) ){
                        tracks[i].obj_type = METEOR;
                        tracks[i].a = a;
                        tracks[i].dx = dx;
                        tracks[i].dy = dy;
                        tracks[i].b = y - a * stats1[next].x;
                    }
                    else {
                        if(tracks[i].obj_type == METEOR) {
                            if (!track_all) {
                                clear_index_Track(tracks, i);
                                continue;
                            }
                            else
                                tracks[i].obj_type = NOISE;
                        }
                    }

                    // tracks[i].vitesse[(tracks[i].cur)++] = stats0[tracks[i].end.ID].error;
                    tracks[i].x = tracks[i].end.x;
                    tracks[i].y = tracks[i].end.y;
                    tracks[i].end = stats1[next];
                    tracks[i].time++;
                    stats1[next].track_id = tracks[i].id;
                    update_bounding_box(tracks+i, stats1[next], frame+1);
                } else {
                    //on extrapole si pas finished
                    Track_extrapolate(&tracks[i], theta, tx, ty);
                    // tracks[i].state = TRACK_FINISHED;
                }
            }
        }
    }
}

void insert_new_track(MeteorROI *ROI_list[256], unsigned n_ROI, Track *tracks, int last, int frame, enum Obj_type type)
{
    assert(last < SIZE_MAX_TRACKS);
    assert(n_ROI >= 2);

    MeteorROI *first_ROI       = ROI_list[n_ROI -1];
    MeteorROI *before_last_ROI = ROI_list[       1];
    MeteorROI *last_ROI        = ROI_list[       0];

    tracks[last].id        = last +1;
    tracks[last].begin     = *first_ROI;
    tracks[last].end       = *last_ROI;
    tracks[last].bb_x      = (uint16)ceil((double)((first_ROI->xmin + first_ROI->xmax))/2);
    tracks[last].bb_y      = (uint16)ceil((double)((first_ROI->ymin + first_ROI->ymax))/2);
    tracks[last].time      = n_ROI;
    tracks[last].timestamp = frame - n_ROI;
    tracks[last].state     = TRACK_NEW;
    tracks[last].obj_type  = type;

    if (type != STAR) {
        float dx = (last_ROI->x - before_last_ROI->x);
        float dy = (last_ROI->y - before_last_ROI->y);

        tracks[last].a = (dx==0) ? INF : (dy/dx);
        tracks[last].b = last_ROI->y - tracks[last].a * last_ROI->x;
        tracks[last].dx = dx;
        tracks[last].dy = dy;
    }

    for (unsigned n = 0; n < n_ROI; n++) {
        ROI_list[n]->track_id = tracks[last].id;
        update_bounding_box(&tracks[last], *ROI_list[n], frame -n);
    }
}

void fill_ROI_list(MeteorROI *ROI_list[256], const unsigned n_ROI, MeteorROI *last_ROI, const unsigned frame) {
    assert(n_ROI < 256);
    unsigned cpt = 0;

    ROI_list[cpt++] = last_ROI;
    int k = search_buf_stat(last_ROI->prev, frame);
    for (int f = 1; f < n_ROI; f++) {
        if (k != -1) {
            ROI_list[cpt++] = &buffer[k].stats0;
            k = search_buf_stat(buffer[k].stats0.prev, frame-f);
        } else {
            fprintf(stderr, "(EE) This should never happen ('k' = -1, 'f' = %d.\n", f);
            exit(-1);
        }
    }
    assert(cpt == n_ROI);
}

void Tracking(MeteorROI *stats0, MeteorROI *stats1, Track *tracks, int nc0, int nc1, int frame, int *last, int *offset, int theta, int tx, int ty, int r_extrapol, int d_line, float diff_deviation, int track_all, int frame_star)
{
    MeteorROI *ROI_list[256];

    double errMoy = errorMoy(stats0, nc0);
    double eType = ecartType(stats0, nc0, errMoy);

    for(int i = 1; i <= nc0; i++) {
        float32 e = stats0[i].error;
        int asso = stats0[i].next;
        if (asso) {
            // si mouvement detecté
            if (fabs(e-errMoy) > diff_deviation * eType) {
                if (stats0[i].state) {
                    PUTS("EXTRAPOLATED");
                    continue; // Extrapolated
                }
                stats0[i].motion = 1; // debug
                stats0[i].time_motion++;
                stats1[stats0[i].next].time_motion = stats0[i].time_motion ;
                if (stats0[i].time_motion == 1) {
                    // stocker dans un buf pour savoir si au moins sur 3 frames
                    insert_buffer(stats0[i], stats1[stats0[i].next], frame);
                }
                if (stats0[i].time_motion == 2) {
                    int j;
                    // mouvement sur 3 frames donc création de track + suppression du buff
                    for (j = *offset; j <= *last; j++)
                        if (tracks[j].end.ID == stats0[i].ID && tracks[j].end.x == stats0[i].x)
                            break;
                    if (j == *last +1 || *last == -1) {
                        fill_ROI_list(ROI_list, 2, &stats0[i], frame);
                        (*last)++;
                        insert_new_track(ROI_list, 2, tracks, *last, frame, METEOR);
                    }
                }
            }
            else if (track_all) {
                stats0[i].time++;
                stats1[stats0[i].next].time = stats0[i].time;
                idisp(frame_star);
                if(stats0[i].time == frame_star) {
                    fill_ROI_list(ROI_list, frame_star, &stats0[i], frame);
                    (*last)++;
                    insert_new_track(ROI_list, frame_star, tracks, *last, frame, STAR);
                }
                else
                    insert_buffer(stats0[i], stats1[stats0[i].next], frame);
            }
        }
    }

    // parcourir les track et update si besoin
    updateTrack(tracks, stats0, stats1, nc1, frame, offset, last, theta, tx, ty, r_extrapol, d_line, track_all);
    clear_buffer_history(frame, frame_star);
}
