#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#define MAP_WIDTH  70
#define MAP_HEIGHT 20
#define MAX_ROOMS  8
#define MAX_ITEMS  12
#define MAX_MOBS   10
#define LOG_LINES  4

// Tile definitions
#define TILE_WALL  '#'
#define TILE_FLOOR '.'
#define TILE_DOOR  '+'
#define TILE_STAIR '>'
#define TILE_EMPTY ' '

// ANSI Color Codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define COLOR_GRAY    "\033[0;90m"

typedef struct {
    int x, y;
    int w, h;
} Room;

typedef enum {
    ITEM_HEALTH,
    ITEM_STRENGTH,
    ITEM_DEFENSE,
    ITEM_GOLD
} ItemType;

typedef struct {
    int x, y;
    ItemType type;
    int value;
    bool active;
} Item;

typedef struct {
    int x, y;
    char symbol;
    char name[16];
    int hp, max_hp;
    int attack;
    int exp_reward;
    bool alive;
} Monster;

typedef struct {
    int x, y;
    int hp, max_hp;
    int attack;
    int defense;
    int level;
    int exp;
    int next_level_exp;
    int gold;
    int dungeon_level;
    int kills;
} Player;

typedef struct {
    char tiles[MAP_HEIGHT][MAP_WIDTH];
    bool explored[MAP_HEIGHT][MAP_WIDTH];
    Room rooms[MAX_ROOMS];
    int room_count;
    Monster monsters[MAX_MOBS];
    int mob_count;
    Item items[MAX_ITEMS];
    int item_count;
    int stair_x, stair_y;
} Dungeon;

typedef struct {
    Player player;
    Dungeon dungeon;
    char log[LOG_LINES][80];
    int log_count;
    bool running;
    bool auto_mode;
    int turns_played;
} Game;

static struct termios orig_termios;
static bool termios_set = false;

static void restore_terminal(void) {
    if (termios_set) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        printf("\033[?25h"); // Show cursor
        fflush(stdout);
        termios_set = false;
    }
}

static void setup_terminal(void) {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &orig_termios);
    termios_set = true;
    atexit(restore_terminal);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?25l"); // Hide cursor
    fflush(stdout);
}

static void add_log(Game *g, const char *msg) {
    if (g->log_count < LOG_LINES) {
        strncpy(g->log[g->log_count++], msg, 79);
    } else {
        for (int i = 1; i < LOG_LINES; i++) {
            strcpy(g->log[i - 1], g->log[i]);
        }
        strncpy(g->log[LOG_LINES - 1], msg, 79);
    }
}

static int rand_range(int min, int max) {
    if (min >= max) return min;
    return min + rand() % (max - min + 1);
}

static bool rooms_overlap(const Room *a, const Room *b) {
    return (a->x <= b->x + b->w && a->x + a->w >= b->x &&
            a->y <= b->y + b->h && a->y + a->h >= b->y);
}

static void carve_room(Dungeon *d, const Room *r) {
    for (int y = r->y; y < r->y + r->h; y++) {
        for (int x = r->x; x < r->x + r->w; x++) {
            if (y >= 0 && y < MAP_HEIGHT && x >= 0 && x < MAP_WIDTH) {
                d->tiles[y][x] = TILE_FLOOR;
            }
        }
    }
}

static void carve_h_corridor(Dungeon *d, int x1, int x2, int y) {
    int start = x1 < x2 ? x1 : x2;
    int end   = x1 < x2 ? x2 : x1;
    for (int x = start; x <= end; x++) {
        if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
            d->tiles[y][x] = TILE_FLOOR;
        }
    }
}

static void carve_v_corridor(Dungeon *d, int y1, int y2, int x) {
    int start = y1 < y2 ? y1 : y2;
    int end   = y1 < y2 ? y2 : y1;
    for (int y = start; y <= end; y++) {
        if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
            d->tiles[y][x] = TILE_FLOOR;
        }
    }
}

static void spawn_monsters(Game *g) {
    Dungeon *d = &g->dungeon;
    d->mob_count = 0;
    int depth = g->player.dungeon_level;

    for (int i = 1; i < d->room_count && d->mob_count < MAX_MOBS; i++) {
        Room *r = &d->rooms[i];
        int num = rand_range(1, 2);
        for (int j = 0; j < num && d->mob_count < MAX_MOBS; j++) {
            Monster *m = &d->monsters[d->mob_count++];
            m->x = rand_range(r->x + 1, r->x + r->w - 2);
            m->y = rand_range(r->y + 1, r->y + r->h - 2);
            m->alive = true;

            int roll = rand() % 100;
            if (roll < 40) {
                strcpy(m->name, "Goblin");
                m->symbol = 'g';
                m->hp = m->max_hp = 10 + depth * 3;
                m->attack = 3 + depth;
                m->exp_reward = 15 + depth * 5;
            } else if (roll < 75) {
                strcpy(m->name, "Skeleton");
                m->symbol = 's';
                m->hp = m->max_hp = 15 + depth * 4;
                m->attack = 5 + depth;
                m->exp_reward = 25 + depth * 8;
            } else if (roll < 95) {
                strcpy(m->name, "Orc");
                m->symbol = 'o';
                m->hp = m->max_hp = 25 + depth * 5;
                m->attack = 8 + depth * 2;
                m->exp_reward = 40 + depth * 10;
            } else {
                strcpy(m->name, "Dragon");
                m->symbol = 'D';
                m->hp = m->max_hp = 50 + depth * 10;
                m->attack = 14 + depth * 3;
                m->exp_reward = 100 + depth * 25;
            }
        }
    }
}

static void spawn_items(Game *g) {
    Dungeon *d = &g->dungeon;
    d->item_count = 0;

    for (int i = 1; i < d->room_count && d->item_count < MAX_ITEMS; i++) {
        Room *r = &d->rooms[i];
        if (rand() % 100 < 70) {
            Item *it = &d->items[d->item_count++];
            it->x = rand_range(r->x + 1, r->x + r->w - 2);
            it->y = rand_range(r->y + 1, r->y + r->h - 2);
            it->active = true;

            int roll = rand() % 100;
            if (roll < 40) {
                it->type = ITEM_HEALTH;
                it->value = 20 + g->player.dungeon_level * 5;
            } else if (roll < 65) {
                it->type = ITEM_GOLD;
                it->value = rand_range(10, 50) * g->player.dungeon_level;
            } else if (roll < 85) {
                it->type = ITEM_STRENGTH;
                it->value = 2;
            } else {
                it->type = ITEM_DEFENSE;
                it->value = 1;
            }
        }
    }
}

static void generate_dungeon(Game *g) {
    Dungeon *d = &g->dungeon;
    memset(d->tiles, TILE_WALL, sizeof(d->tiles));
    memset(d->explored, 0, sizeof(d->explored));
    d->room_count = 0;

    for (int i = 0; i < 20 && d->room_count < MAX_ROOMS; i++) {
        Room r;
        r.w = rand_range(6, 12);
        r.h = rand_range(4, 7);
        r.x = rand_range(1, MAP_WIDTH - r.w - 1);
        r.y = rand_range(1, MAP_HEIGHT - r.h - 1);

        bool overlap = false;
        for (int j = 0; j < d->room_count; j++) {
            if (rooms_overlap(&r, &d->rooms[j])) {
                overlap = true;
                break;
            }
        }

        if (!overlap) {
            carve_room(d, &r);
            if (d->room_count > 0) {
                Room *prev = &d->rooms[d->room_count - 1];
                int prev_cx = prev->x + prev->w / 2;
                int prev_cy = prev->y + prev->h / 2;
                int cur_cx  = r.x + r.w / 2;
                int cur_cy  = r.y + r.h / 2;

                if (rand() % 2) {
                    carve_h_corridor(d, prev_cx, cur_cx, prev_cy);
                    carve_v_corridor(d, prev_cy, cur_cy, cur_cx);
                } else {
                    carve_v_corridor(d, prev_cy, cur_cy, prev_cx);
                    carve_h_corridor(d, prev_cx, cur_cx, cur_cy);
                }
            }
            d->rooms[d->room_count++] = r;
        }
    }

    // Player starts in first room
    g->player.x = d->rooms[0].x + d->rooms[0].w / 2;
    g->player.y = d->rooms[0].y + d->rooms[0].h / 2;

    // Stairs in last room
    Room *last = &d->rooms[d->room_count - 1];
    d->stair_x = last->x + last->w / 2;
    d->stair_y = last->y + last->h / 2;
    d->tiles[d->stair_y][d->stair_x] = TILE_STAIR;

    spawn_monsters(g);
    spawn_items(g);
}

static void update_fov(Game *g) {
    int px = g->player.x;
    int py = g->player.y;
    int radius = 5;

    for (int y = py - radius; y <= py + radius; y++) {
        for (int x = px - radius; x <= px + radius; x++) {
            if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
                int dx = x - px;
                int dy = y - py;
                if (dx * dx + dy * dy <= radius * radius) {
                    g->dungeon.explored[y][x] = true;
                }
            }
        }
    }
}

static Monster *get_monster_at(Dungeon *d, int x, int y) {
    for (int i = 0; i < d->mob_count; i++) {
        if (d->monsters[i].alive && d->monsters[i].x == x && d->monsters[i].y == y) {
            return &d->monsters[i];
        }
    }
    return NULL;
}

static Item *get_item_at(Dungeon *d, int x, int y) {
    for (int i = 0; i < d->item_count; i++) {
        if (d->items[i].active && d->items[i].x == x && d->items[i].y == y) {
            return &d->items[i];
        }
    }
    return NULL;
}

static void level_up_check(Game *g) {
    Player *p = &g->player;
    while (p->exp >= p->next_level_exp) {
        p->level++;
        p->exp -= p->next_level_exp;
        p->next_level_exp = (int)(p->next_level_exp * 1.5f);
        p->max_hp += 10;
        p->hp = p->max_hp;
        p->attack += 3;
        p->defense += 1;

        char buf[80];
        snprintf(buf, sizeof(buf), COLOR_YELLOW "LEVEL UP! Reached Level %d! HP restored." COLOR_RESET, p->level);
        add_log(g, buf);
    }
}

static void player_attack(Game *g, Monster *m) {
    Player *p = &g->player;
    int damage = p->attack + rand_range(-2, 2);
    if (damage < 1) damage = 1;

    m->hp -= damage;
    char buf[80];
    snprintf(buf, sizeof(buf), "You hit %s for %d damage!", m->name, damage);
    add_log(g, buf);

    if (m->hp <= 0) {
        m->alive = false;
        p->kills++;
        p->exp += m->exp_reward;
        snprintf(buf, sizeof(buf), COLOR_GREEN "Defeated %s! (+%d EXP)" COLOR_RESET, m->name, m->exp_reward);
        add_log(g, buf);
        level_up_check(g);
    }
}

static void monster_turn(Game *g) {
    Dungeon *d = &g->dungeon;
    Player *p = &g->player;

    for (int i = 0; i < d->mob_count; i++) {
        Monster *m = &d->monsters[i];
        if (!m->alive) continue;

        int dx = abs(m->x - p->x);
        int dy = abs(m->y - p->y);

        // Attack if adjacent
        if ((dx == 1 && dy == 0) || (dx == 0 && dy == 1)) {
            int damage = m->attack - p->defense;
            if (damage < 1) damage = 1;
            p->hp -= damage;

            char buf[80];
            snprintf(buf, sizeof(buf), COLOR_RED "%s hits you for %d damage!" COLOR_RESET, m->name, damage);
            add_log(g, buf);

            if (p->hp <= 0) {
                add_log(g, COLOR_RED "You have been slain... GAME OVER!" COLOR_RESET);
                g->running = false;
                return;
            }
        } else if (dx + dy < 8) {
            // Chase player
            int step_x = (p->x > m->x) ? 1 : (p->x < m->x ? -1 : 0);
            int step_y = (p->y > m->y) ? 1 : (p->y < m->y ? -1 : 0);
            int nx = m->x + step_x;
            int ny = m->y + step_y;

            if (d->tiles[ny][nx] == TILE_FLOOR && !get_monster_at(d, nx, ny) && !(nx == p->x && ny == p->y)) {
                m->x = nx;
                m->y = ny;
            }
        }
    }
}

static void move_player(Game *g, int dx, int dy) {
    int nx = g->player.x + dx;
    int ny = g->player.y + dy;

    if (nx < 0 || nx >= MAP_WIDTH || ny < 0 || ny >= MAP_HEIGHT) return;

    Monster *m = get_monster_at(&g->dungeon, nx, ny);
    if (m) {
        player_attack(g, m);
        monster_turn(g);
        g->turns_played++;
        return;
    }

    if (g->dungeon.tiles[ny][nx] == TILE_WALL) {
        return;
    }

    g->player.x = nx;
    g->player.y = ny;

    // Check items
    Item *it = get_item_at(&g->dungeon, nx, ny);
    if (it) {
        char buf[80];
        if (it->type == ITEM_HEALTH) {
            g->player.hp += it->value;
            if (g->player.hp > g->player.max_hp) g->player.hp = g->player.max_hp;
            snprintf(buf, sizeof(buf), COLOR_GREEN "Found Health Potion! Restored %d HP." COLOR_RESET, it->value);
        } else if (it->type == ITEM_GOLD) {
            g->player.gold += it->value;
            snprintf(buf, sizeof(buf), COLOR_YELLOW "Found %d Gold!" COLOR_RESET, it->value);
        } else if (it->type == ITEM_STRENGTH) {
            g->player.attack += it->value;
            snprintf(buf, sizeof(buf), COLOR_MAGENTA "Found Sword Upgrade! (+%d ATK)" COLOR_RESET, it->value);
        } else if (it->type == ITEM_DEFENSE) {
            g->player.defense += it->value;
            snprintf(buf, sizeof(buf), COLOR_CYAN "Found Shield Upgrade! (+%d DEF)" COLOR_RESET, it->value);
        }
        add_log(g, buf);
        it->active = false;
    }

    // Check stairs
    if (nx == g->dungeon.stair_x && ny == g->dungeon.stair_y) {
        g->player.dungeon_level++;
        char buf[80];
        snprintf(buf, sizeof(buf), COLOR_CYAN "You descend deeper into Level %d..." COLOR_RESET, g->player.dungeon_level);
        add_log(g, buf);
        generate_dungeon(g);
    }

    monster_turn(g);
    g->turns_played++;
}

static void render(Game *g) {
    update_fov(g);
    printf("\033[H\033[2J"); // Clear screen

    printf(COLOR_YELLOW "=== DUNGEON OF WOODY ===  Floor: %d  |  Turn: %d\n" COLOR_RESET,
           g->player.dungeon_level, g->turns_played);

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (x == g->player.x && y == g->player.y) {
                printf(COLOR_GREEN "@" COLOR_RESET);
                continue;
            }

            Monster *m = get_monster_at(&g->dungeon, x, y);
            if (m && g->dungeon.explored[y][x]) {
                printf(COLOR_RED "%c" COLOR_RESET, m->symbol);
                continue;
            }

            Item *it = get_item_at(&g->dungeon, x, y);
            if (it && g->dungeon.explored[y][x]) {
                if (it->type == ITEM_HEALTH)    printf(COLOR_GREEN "!" COLOR_RESET);
                else if (it->type == ITEM_GOLD) printf(COLOR_YELLOW "$" COLOR_RESET);
                else if (it->type == ITEM_STRENGTH) printf(COLOR_MAGENTA "^" COLOR_RESET);
                else printf(COLOR_CYAN "]" COLOR_RESET);
                continue;
            }

            if (g->dungeon.explored[y][x]) {
                char t = g->dungeon.tiles[y][x];
                if (t == TILE_STAIR) printf(COLOR_CYAN ">" COLOR_RESET);
                else if (t == TILE_WALL) printf(COLOR_GRAY "#" COLOR_RESET);
                else printf(".");
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }

    Player *p = &g->player;
    printf(COLOR_WHITE "HP: %d/%d  |  ATK: %d  |  DEF: %d  |  LVL: %d  |  EXP: %d/%d  |  GOLD: %d  |  KILLS: %d\n" COLOR_RESET,
           p->hp, p->max_hp, p->attack, p->defense, p->level, p->exp, p->next_level_exp, p->gold, p->kills);

    printf("----------------------------------------------------------------------\n");
    for (int i = 0; i < LOG_LINES; i++) {
        if (i < g->log_count) {
            printf("%s\n", g->log[i]);
        } else {
            printf("\n");
        }
    }
    printf(COLOR_GRAY "[WASD/Arrows: Move | Q: Quit]\n" COLOR_RESET);
    fflush(stdout);
}

static void init_game(Game *g, bool auto_mode) {
    memset(g, 0, sizeof(*g));
    g->auto_mode = auto_mode;
    g->running = true;
    g->player.hp = g->player.max_hp = 35;
    g->player.attack = 8;
    g->player.defense = 2;
    g->player.level = 1;
    g->player.next_level_exp = 30;
    g->player.dungeon_level = 1;

    add_log(g, "Welcome to the Dungeon of Woody! Reach the stairs (>) to descend.");
    generate_dungeon(g);
}

// Automated bot for headless test execution
static void run_auto_test(Game *g, int max_turns) {
    printf("[*] Starting automated dungeon test (%d turns simulation)...\n", max_turns);
    int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    while (g->running && g->turns_played < max_turns) {
        // Simple AI: if monster nearby, move toward it, else move toward stairs
        int target_x = g->dungeon.stair_x;
        int target_y = g->dungeon.stair_y;

        for (int i = 0; i < g->dungeon.mob_count; i++) {
            if (g->dungeon.monsters[i].alive) {
                target_x = g->dungeon.monsters[i].x;
                target_y = g->dungeon.monsters[i].y;
                break;
            }
        }

        int best_dx = 0, best_dy = 0;
        int best_dist = 999999;

        for (int i = 0; i < 4; i++) {
            int nx = g->player.x + dirs[i][0];
            int ny = g->player.y + dirs[i][1];
            if (nx >= 0 && nx < MAP_WIDTH && ny >= 0 && ny < MAP_HEIGHT &&
                g->dungeon.tiles[ny][nx] != TILE_WALL) {
                int d = abs(nx - target_x) + abs(ny - target_y);
                if (d < best_dist) {
                    best_dist = d;
                    best_dx = dirs[i][0];
                    best_dy = dirs[i][1];
                }
            }
        }

        if (best_dx == 0 && best_dy == 0) {
            int r = rand() % 4;
            best_dx = dirs[r][0];
            best_dy = dirs[r][1];
        }

        move_player(g, best_dx, best_dy);
    }

    printf("[+] Simulation completed successfully!\n");
    printf("    Turns played : %d\n", g->turns_played);
    printf("    Floor reached: %d\n", g->player.dungeon_level);
    printf("    Player Level : %d\n", g->player.level);
    printf("    Monsters slain: %d\n", g->player.kills);
    printf("    Gold gathered: %d\n", g->player.gold);
    printf("    Status       : %s\n", g->player.hp > 0 ? "SURVIVED" : "DIED VALIANTLY");
}

int main(int argc, char **argv) {
    srand(time(NULL) ^ getpid());

    bool test_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--test") == 0 || strcmp(argv[i], "-t") == 0) {
            test_mode = true;
            break;
        }
    }

    if (!isatty(STDIN_FILENO)) {
        test_mode = true;
    }

    Game g;
    init_game(&g, test_mode);

    if (test_mode) {
        run_auto_test(&g, 200);
        return 0;
    }

    setup_terminal();

    while (g.running) {
        render(&g);
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        if (c == 'q' || c == 'Q') {
            break;
        } else if (c == 'w' || c == 'W') {
            move_player(&g, 0, -1);
        } else if (c == 's' || c == 'S') {
            move_player(&g, 0, 1);
        } else if (c == 'a' || c == 'A') {
            move_player(&g, -1, 0);
        } else if (c == 'd' || c == 'D') {
            move_player(&g, 1, 0);
        } else if (c == '\033') { // Arrow keys
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[0] == '[') {
                    if (seq[1] == 'A') move_player(&g, 0, -1);
                    else if (seq[1] == 'B') move_player(&g, 0, 1);
                    else if (seq[1] == 'C') move_player(&g, 1, 0);
                    else if (seq[1] == 'D') move_player(&g, -1, 0);
                }
            }
        }
    }

    restore_terminal();
    printf("\nThanks for playing Dungeon of Woody!\n");
    return 0;
}
