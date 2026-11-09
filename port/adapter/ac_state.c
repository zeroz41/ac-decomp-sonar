/* The game's state at each tick boundary, for --state-hashes (sonarlib docs/roadmap.md, M6 step 9f3b; port/NOTES.md,
   State hashes): the structures the port registers with sonar/state.h, which the boundary hashes while no game thread
   runs, and the listener that writes each boundary's tick and hash to the file, and with --plant adds 1 to one byte
   of the save at its tick. The entries, in order: common_data without its pointers, qrand's two words, the game's
   clocks and play's actors. The audio's state, the emulator's and the adapter's are left out. */
#include "game.h"
#include "libc64/qrand.h"
#include "m_actor.h"
#include "m_common_data.h"
#include "m_play.h"

#include "ac_port.h"

#include <sonar/state.h>

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* common_data's top-level fields after the save, by their host offsets and sizes, in order. Left out, as they are or
   hold pointers: clip, now_private, now_home, npc_schedule (each schedule's data_table), start_demo_request (its actor
   and proc), pluss_bridge_pos and carde_program_p. The gaps between the fields are left out with them. */
#define FIELD(name) { offsetof(common_data_t, name), sizeof(((common_data_t*)0)->name) }
static const struct {
    size_t offset;
    size_t size;
} fields[] = {
    FIELD(game_started),
    FIELD(field_type),
    FIELD(field_draw_type),
    FIELD(player_no),
    FIELD(last_scene_no),
    FIELD(player_data_mode),
    FIELD(time),
    FIELD(map_flag),
    FIELD(fish_location),
    FIELD(npc_is_summercamper),
    FIELD(player_select_animal_no),
    FIELD(_26148),
    FIELD(transition),
    FIELD(bg_item_type),
    FIELD(bg_item_profile),
    FIELD(_26154),
    FIELD(npclist),
    FIELD(island_npclist),
    FIELD(house_owner_name),
    FIELD(last_field_id),
    FIELD(in_initial_block),
    FIELD(submenu_disabled),
    FIELD(sunlight_flag),
    FIELD(train_flag),
    FIELD(unused_mail_26522),
    FIELD(_2664E),
    FIELD(_26650),
    FIELD(unused_mail_name_26652),
    FIELD(npc_chg_cloth),
    FIELD(_pad_2666A),
    FIELD(weather),
    FIELD(weather_intensity),
    FIELD(weather_time),
    FIELD(wind),
    FIELD(wind_speed),
    FIELD(special_event_common),
    FIELD(quest),
    FIELD(scene_from_title_demo),
    FIELD(npc_walk),
    FIELD(event_npc),
    FIELD(mask_npc),
    FIELD(snowman_msg_id),
    FIELD(money_power),
    FIELD(goods_power),
    FIELD(door_data),
    FIELD(structure_exit_door_data),
    FIELD(event_door_data),
    FIELD(famicom_emu_exit_door_data),
    FIELD(remove_cut_tree_info_bitfield),
    FIELD(floor_idx),
    FIELD(demo_profiles),
    FIELD(copy_protect),
    FIELD(event_keep_flags),
    FIELD(_285A8),
    FIELD(player_warp_request),
    FIELD(player_actor_exists),
    FIELD(complete_payment_type),
    FIELD(player_decoy_flag),
    FIELD(axe_damage),
    FIELD(make_npc2_actor),
    FIELD(event_id),
    FIELD(event_title_flags),
    FIELD(event_title_fade_in_progress),
    FIELD(event_common),
    FIELD(current_famicom_rom),
    FIELD(famicom_287F9),
    FIELD(_287FA),
    FIELD(player_bee_swell_flag),
    FIELD(player_bee_chase_flag),
    FIELD(goki_shocked_flag),
    FIELD(time_changed_flag),
    FIELD(unable_wade),
    FIELD(fail_emu_flag),
    FIELD(train_coming_flag),
    FIELD(train_exists_flag),
    FIELD(train_control_state),
    FIELD(train_last_control_state),
    FIELD(train_signal),
    FIELD(train_day),
    FIELD(train_action),
    FIELD(train_timer),
    FIELD(train_start_timer),
    FIELD(train_speed),
    FIELD(train_position),
    FIELD(unused_02885C),
    FIELD(unused_028860),
    FIELD(unused_028864),
    FIELD(unused_028866),
    FIELD(reset_flag),
    FIELD(reset_type),
    FIELD(force_mail_delivery_flag),
    FIELD(post_girl_npc_type),
    FIELD(ball_pos),
    FIELD(ball_type),
    FIELD(auto_nwrite_count),
    FIELD(auto_nwrite_year),
    FIELD(save_error_type),
    FIELD(train_approaching_flag),
    FIELD(buried_treasure_flag),
    FIELD(spnpc_first_talk_flags),
    FIELD(needlework_first_talk_flags),
    FIELD(event_notification_active),
    FIELD(newly_set_time),
    FIELD(old_time),
    FIELD(balloon_state),
    FIELD(balloon_last_spawn_min),
    FIELD(balloon_spawn_percent),
    FIELD(tanuki_shop_status),
    FIELD(pad_connected),
    FIELD(unk288A1),
    FIELD(current_sound_effect),
    FIELD(_288a4),
    FIELD(transfer_island),
    FIELD(agb_island),
    FIELD(auto_nwrite_set),
    FIELD(select_last_select_no),
    FIELD(select_last_top_no),
    FIELD(travel_persistent_data),
    FIELD(island_weather),
    FIELD(island_weather_intensity),
    FIELD(sunburn_time),
    FIELD(memcard_slot),
    FIELD(my_room_message_control_flags),
    FIELD(can_look_goki_count),
    FIELD(rainbow_opacity),
    FIELD(event_flags),
    FIELD(auto_nwrite_time),
    FIELD(rhythym_updated),
    FIELD(_2dbe1),
    FIELD(hem_visible),
    FIELD(carde_program_size),
    FIELD(unk_nook_present_count),
    FIELD(pad),
};
#undef FIELD

static FILE* file;       /* --state-hashes's file */
static const char* path; /* and its path */
static BOOL plant;       /* --plant */
static u64 plant_tick;   /* and its tick */

/* The save's bytes, padding and all, as the card holds them, then each field above. */
static void hash_common_data(sl_state_hasher* hasher, void* user) {
    size_t i;
    (void)user;
    sl_state_hash_bytes(hasher, &common_data.save, sizeof common_data.save);
    for (i = 0; i < sizeof fields / sizeof fields[0]; i++)
        sl_state_hash_bytes(hasher, (const u8*)&common_data + fields[i].offset, fields[i].size);
}

/* The game's frame_counter while graph_proc has a game, and play's game_frame while that game is play. */
static void hash_clocks(sl_state_hasher* hasher, void* user) {
    (void)user;
    if (game_class_p == NULL)
        return;
    sl_state_hash_bytes(hasher, &game_class_p->frame_counter, sizeof game_class_p->frame_counter);
    if (game_class_p->exec == play_main) {
        const GAME_PLAY* play = (const GAME_PLAY*)game_class_p;
        sl_state_hash_bytes(hasher, &play->game_frame, sizeof play->game_frame);
    }
}

#define ACTOR_FIELD(actor, name) sl_state_hash_bytes(hasher, &(actor)->name, sizeof(actor)->name)

/* Play's eight lists in order, each its count, then each actor's fields from id to ground_y in list order, while the
   game is play. A boundary falls only while every game thread waits in os, and the lists change only between those
   waits, so the walk finds them whole even inside play's constructor or destructor: graph_proc's bzero leaves them
   empty until Actor_info_ct fills them, an actor is linked only after Actor_init_actor_class has set it and unlinked
   before it is freed, and Actor_info_dt leaves them empty (port/NOTES.md, State hashes). */
static void hash_actors(sl_state_hasher* hasher, void* user) {
    const GAME_PLAY* play;
    int part;
    (void)user;
    if (game_class_p == NULL || game_class_p->exec != play_main)
        return;
    play = (const GAME_PLAY*)game_class_p;
    for (part = 0; part < ACTOR_PART_NUM; part++) {
        const Actor_list* list = &play->actor_info.list[part];
        const ACTOR* actor;
        sl_state_hash_bytes(hasher, &list->num_actors, sizeof list->num_actors);
        for (actor = list->actor; actor != NULL; actor = actor->next_actor) {
            ACTOR_FIELD(actor, id);
            ACTOR_FIELD(actor, part);
            ACTOR_FIELD(actor, restore_fg);
            ACTOR_FIELD(actor, scene_id);
            ACTOR_FIELD(actor, npc_id);
            ACTOR_FIELD(actor, block_x);
            ACTOR_FIELD(actor, block_z);
            ACTOR_FIELD(actor, move_actor_list_idx);
            ACTOR_FIELD(actor, home);
            ACTOR_FIELD(actor, state_bitfield);
            ACTOR_FIELD(actor, actor_specific);
            ACTOR_FIELD(actor, data_bank_id);
            ACTOR_FIELD(actor, world);
            ACTOR_FIELD(actor, last_world_position);
            ACTOR_FIELD(actor, eye);
            ACTOR_FIELD(actor, scale);
            ACTOR_FIELD(actor, position_speed);
            ACTOR_FIELD(actor, speed);
            ACTOR_FIELD(actor, gravity);
            ACTOR_FIELD(actor, max_velocity_y);
            ACTOR_FIELD(actor, ground_y);
        }
    }
}

#undef ACTOR_FIELD

/* Each boundary's line, flushed, as runs end with _Exit; then, at --plant's tick, after its hash, 1 added to the
   save's pad_1C[0], which the town replay neither reads nor writes from then on (port/NOTES.md, State hashes). A
   write that fails ends the run. */
static void write_hash(uint64_t tick, uint64_t hash, void* user) {
    (void)user;
    if (fprintf(file, "%" PRIu64 " %016" PRIx64 "\n", tick, hash) < 0 || fflush(file) != 0) {
        fprintf(stderr, "ac_port: %s: cannot write the state hashes: %s\n", path, strerror(errno));
        _Exit(1);
    }
    if (plant && tick == plant_tick)
        common_data.save.save.pad_1C[0]++;
}

BOOL ac_state_start(const char* hashes, BOOL plant_given, u64 plant_at) {
    static const char* const names[] = { "common_data", "qrand_idum", "qrand_itemp", "game_clocks", "actors" };
    const u32* idum;
    const u32* itemp;
    sl_state_desc descs[5];
    sl_result result = SL_OK;
    int i;
    file = fopen(hashes, "wb");
    if (file == NULL) {
        fprintf(stderr, "ac_port: %s: %s\n", hashes, strerror(errno));
        return FALSE;
    }
    path = hashes;
    plant = plant_given;
    plant_tick = plant_at;
    qrand_state(&idum, &itemp);
    memset(descs, 0, sizeof descs);
    descs[0].hash = hash_common_data;
    descs[1].data = idum;
    descs[1].size = sizeof *idum;
    descs[2].data = itemp;
    descs[2].size = sizeof *itemp;
    descs[3].hash = hash_clocks;
    descs[4].hash = hash_actors;
    for (i = 0; i < 5 && result == SL_OK; i++) {
        descs[i].name = names[i];
        result = sl_state_add(&descs[i]);
    }
    if (result == SL_OK)
        result = sl_state_listen(write_hash, NULL);
    if (result != SL_OK) {
        fprintf(stderr, "ac_port: the state hashes: %s\n", sl_result_string(result));
        return FALSE;
    }
    return TRUE;
}
