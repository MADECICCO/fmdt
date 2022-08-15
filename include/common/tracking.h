// ------------------
// --- Tracking.h ---
// ------------------

#pragma once

#include <stdint.h>

#include "features.h"

/*
 * Maxime Millet
 * Clara Ciocan/ Mathuran Kandeepan
 */

// Enums
enum obj_e { UNKNOWN = 0, STAR, METEOR, NOISE, N_OBJECTS };
enum color_e { MISC = 0, GRAY, GREEN, RED, PURPLE, ORANGE, BLUE, YELLOW, N_COLORS };
enum state_e { TRACK_NEW = 1, TRACK_FINISHED, TRACK_EXTRAPOLATED, TRACK_UPDATED, TRACK_LOST };

#define METEOR_COLOR GREEN
#define STAR_COLOR PURPLE
#define NOISE_COLOR ORANGE
#define UNKNOWN_COLOR GRAY

#define METEOR_STR "meteor"
#define STAR_STR "star"
#define NOISE_STR "noise"
#define UNKNOWN_STR "unknown"

typedef struct track {
    // unsigned state;
    unsigned timestamp;
    uint16_t id;
    ROI_t begin;
    ROI_t end;
    float x;
    float y;
    float dx;
    float dy;
    uint32_t time;
    uint16_t bb_x;
    uint16_t bb_y;
    uint16_t rx;
    uint16_t ry;
    // equation de la droite : y = ax + b
    float a;
    float b;
    float xmin;
    float xmax;
    float ymin;
    float ymax;
    int state;
    enum obj_e obj_type;
    // Resultat validation (bad design)
    uint8_t is_valid;
    // Juste pour afficher les vitesses
    float vitesse[300];
    int cur;
} track_t;

typedef struct {
    ROI_t stats0;
    ROI_t stats1;
    int frame;
} ROIx2_t;

typedef struct BB_t {
    uint16_t bb_x;
    uint16_t bb_y;
    uint16_t rx;
    uint16_t ry;
    uint16_t track_id;
    struct BB_t* next;
} BB_t;

extern enum color_e g_obj_to_color[N_OBJECTS];
extern char g_obj_to_string[N_OBJECTS][64];
extern char g_obj_to_string_with_spaces[N_OBJECTS][64];

void tracking_init_global_data();
enum obj_e tracking_string_to_obj_type(const char* string);
void tracking_init_tracks(track_t* tracks, int n);
void tracking_init_BB_array(BB_t** BB_array);
void tracking_free_BB_array(BB_t** BB_array);
void tracking_perform(ROI_t* stats0, ROI_t* stats1, ROIx2_t *ROI_history, track_t* tracks, BB_t** BB_array, int nc0,
                      int nc1, int frame, int* tracks_cnt, int* offset, int theta, int tx, int ty, int r_extrapol,
                      int d_line, float diff_deviation, int track_all, int frame_star);

// return the real number of tracks
unsigned tracking_count_objects(const track_t* tracks, const int n_tracks, unsigned* n_stars, unsigned* n_meteors,
                                 unsigned* n_noise);

// void tracking_print_array_BB(BB_t** tabBB, int n);
void tracking_print_tracks(FILE* f, track_t* tracks, int n);
// void tracking_print_buffer(ROIx2_t* buffer, int n);
void tracking_parse_tracks(const char* filename, track_t* tracks, int* n);
// void tracking_save_tracks(const char* filename, track_t* tracks, int n);
void tracking_save_array_BB(const char* filename, BB_t** BB_array, track_t* tracks, int n, int track_all);