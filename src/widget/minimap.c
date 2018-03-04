#include "minimap.h"

#include "building/building.h"
#include "city/view.h"
#include "figure/figure.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "widget/sidebar.h"

#include "Data/CityView.h"
#include "Data/State.h"

enum {
    FIGURE_COLOR_NONE = 0,
    FIGURE_COLOR_SOLDIER = 1,
    FIGURE_COLOR_ENEMY = 2,
    FIGURE_COLOR_WOLF = 3
};

static struct {
    int absolute_x;
    int absolute_y;
    int width_tiles;
    int height_tiles;
    int x_offset;
    int y_offset;
    int width;
    int height;
    color_t enemy_color;
    struct {
        int x;
        int y;
        int grid_offset;
    } mouse;
} data;

static void foreach_map_tile(void (*callback)(int x_view, int y_view, int grid_offset))
{
    int odd = 0;
    int y_abs = data.absolute_y - 4;
    int y_view = data.y_offset - 4;
    for (int y_rel = -4; y_rel < data.height_tiles + 4; y_rel++, y_abs++, y_view++) {
        int x_view;
        if (odd) {
            x_view = data.x_offset - 9;
            odd = 0;
        } else {
            x_view = data.x_offset - 8;
            odd = 1;
        }
        int x_abs = data.absolute_x - 4;
        for (int x_rel = -4; x_rel < data.width_tiles; x_rel++, x_abs++, x_view += 2) {
            if (x_abs >= 0 && x_abs < VIEW_X_MAX && y_abs >= 0 && y_abs < VIEW_Y_MAX) {
                callback(x_view, y_view, ViewToGridOffset(x_abs, y_abs));
            }
        }
    }    
}

static void set_bounds(int x_offset, int y_offset, int width_tiles, int height_tiles)
{
    data.width_tiles = width_tiles;
    data.height_tiles = height_tiles;
    data.x_offset = x_offset;
    data.y_offset = y_offset;
    data.width = 2 * width_tiles;
    data.height = data.height_tiles;
    data.absolute_x = (VIEW_X_MAX - width_tiles) / 2;
    data.absolute_y = (VIEW_Y_MAX - height_tiles) / 2;

    if ((Data_State.map.width - width_tiles) / 2 > 0) {
        if (Data_CityView.xInTiles < data.absolute_x) {
            data.absolute_x = Data_CityView.xInTiles;
        } else if (Data_CityView.xInTiles > width_tiles + data.absolute_x - Data_CityView.widthInTiles) {
            data.absolute_x = Data_CityView.widthInTiles + Data_CityView.xInTiles - width_tiles;
        }
    }
    if ((2 * Data_State.map.height - height_tiles) / 2 > 0) {
        if (Data_CityView.yInTiles < data.absolute_y) {
            data.absolute_y = Data_CityView.yInTiles;
        } else if (Data_CityView.yInTiles > height_tiles + data.absolute_y - Data_CityView.heightInTiles) {
            data.absolute_y = Data_CityView.heightInTiles + Data_CityView.yInTiles - height_tiles;
        }
    }
    // ensure even height
    data.absolute_y &= ~1;
}

static int has_figure_color(figure *f)
{
    int type = f->type;
    if (figure_is_legion(f)) {
        return FIGURE_COLOR_SOLDIER;
    }
    if (figure_is_enemy(f)) {
        return FIGURE_COLOR_ENEMY;
    }
    if (f->type == FIGURE_INDIGENOUS_NATIVE &&
        f->actionState == FIGURE_ACTION_159_NATIVE_ATTACKING) {
        return FIGURE_COLOR_ENEMY;
    }
    if (type == FIGURE_WOLF) {
        return FIGURE_COLOR_WOLF;
    }
    return FIGURE_COLOR_NONE;
}

static int draw_figure(int x_view, int y_view, int grid_offset)
{
    int color_type = map_figure_foreach_until(grid_offset, has_figure_color);
    if (color_type == FIGURE_COLOR_NONE) {
        return 0;
    }
    color_t color = COLOR_BLACK;
    if (color_type == FIGURE_COLOR_SOLDIER) {
        color = COLOR_SOLDIER;
    } else if (color_type == FIGURE_COLOR_ENEMY) {
        color = data.enemy_color;
    }
    graphics_draw_line(x_view, y_view, x_view +1, y_view, color);
    return 1;
}

static void draw_minimap_tile(int x_view, int y_view, int grid_offset)
{
    if (grid_offset < 0) {
        image_draw(image_group(GROUP_MINIMAP_BLACK), x_view, y_view);
        return;
    }

    if (draw_figure(x_view, y_view, grid_offset)) {
        return;
    }
    
    int terrain = map_terrain_get(grid_offset);
    // exception for fort ground: display as empty land
    if (terrain & TERRAIN_BUILDING) {
        if (building_get(map_building_at(grid_offset))->type == BUILDING_FORT_GROUND) {
            terrain = 0;
        }
    }

    if (terrain & TERRAIN_BUILDING) {
        if (map_property_is_draw_tile(grid_offset)) {
            int image_id;
            building *b = building_get(map_building_at(grid_offset));
            if (b->houseSize) {
                image_id = image_group(GROUP_MINIMAP_HOUSE);
            } else if (b->type == BUILDING_RESERVOIR) {
                image_id = image_group(GROUP_MINIMAP_AQUEDUCT) - 1;
            } else {
                image_id = image_group(GROUP_MINIMAP_BUILDING);
            }
            switch (map_property_multi_tile_size(grid_offset)) {
                case 1: image_draw(image_id, x_view, y_view); break;
                case 2: image_draw(image_id + 1, x_view, y_view - 1); break;
                case 3: image_draw(image_id + 2, x_view, y_view - 2); break;
                case 4: image_draw(image_id + 3, x_view, y_view - 3); break;
                case 5: image_draw(image_id + 4, x_view, y_view - 4); break;
            }
        }
    } else {
        int rand = map_random_get(grid_offset);
        int image_id;
        if (terrain & TERRAIN_WATER) {
            image_id = image_group(GROUP_MINIMAP_WATER) + (rand & 3);
        } else if (terrain & TERRAIN_SCRUB) {
            image_id = image_group(GROUP_MINIMAP_TREE) + (rand & 3);
        } else if (terrain & TERRAIN_TREE) {
            image_id = image_group(GROUP_MINIMAP_TREE) + (rand & 3);
        } else if (terrain & TERRAIN_ROCK) {
            image_id = image_group(GROUP_MINIMAP_ROCK) + (rand & 3);
        } else if (terrain & TERRAIN_ELEVATION) {
            image_id = image_group(GROUP_MINIMAP_ROCK) + (rand & 3);
        } else if (terrain & TERRAIN_ROAD) {
            image_id = image_group(GROUP_MINIMAP_ROAD);
        } else if (terrain & TERRAIN_AQUEDUCT) {
            image_id = image_group(GROUP_MINIMAP_AQUEDUCT);
        } else if (terrain & TERRAIN_WALL) {
            image_id = image_group(GROUP_MINIMAP_WALL);
        } else if (terrain & TERRAIN_MEADOW) {
            image_id = image_group(GROUP_MINIMAP_MEADOW) + (rand & 3);
        } else {
            image_id = image_group(GROUP_MINIMAP_EMPTY_LAND) + (rand & 7);
        }
        image_draw(image_id, x_view, y_view);
    }
}

static void draw_viewport_rectangle()
{
    int x_offset = data.x_offset + 2 * (Data_CityView.xInTiles - data.absolute_x) - 2;
    if (x_offset < data.x_offset) {
        x_offset = data.x_offset;
    }
    if (x_offset + 2 * Data_CityView.widthInTiles + 4 > data.x_offset + data.width_tiles) {
        x_offset -= 2;
    }
    int y_offset = data.y_offset + Data_CityView.yInTiles - data.absolute_y + 2;
    graphics_draw_rect(x_offset, y_offset,
        Data_CityView.widthInTiles * 2 + 4,
        Data_CityView.heightInTiles - 4,
        COLOR_YELLOW);
}

void widget_minimap_draw(int x_offset, int y_offset, int width_tiles, int height_tiles)
{
    graphics_set_clip_rectangle(x_offset, y_offset, 2 * width_tiles, height_tiles);
    
    switch (scenario_property_climate()) {
        case CLIMATE_CENTRAL: data.enemy_color = COLOR_ENEMY_CENTRAL; break;
        case CLIMATE_NORTHERN: data.enemy_color = COLOR_ENEMY_NORTHERN; break;
        default: data.enemy_color = COLOR_ENEMY_DESERT; break;
    }

    set_bounds(x_offset, y_offset, width_tiles, height_tiles);
    foreach_map_tile(draw_minimap_tile);
    draw_viewport_rectangle();

    graphics_reset_clip_rectangle();
}

static void update_mouse_grid_offset(int x_view, int y_view, int grid_offset)
{
    if (data.mouse.y == y_view && (data.mouse.x == x_view || data.mouse.x == x_view + 1)) {
        data.mouse.grid_offset = grid_offset < 0 ? 0 : grid_offset;
    }
}

static int get_mouse_grid_offset(const mouse *m)
{
    data.mouse.x = m->x;
    data.mouse.y = m->y;
    data.mouse.grid_offset = 0;
    foreach_map_tile(update_mouse_grid_offset);
    return data.mouse.grid_offset;
}

static int is_in_minimap(const mouse *m)
{
    if (m->x >= data.x_offset && m->x < data.x_offset + data.width &&
        m->y >= data.y_offset && m->y < data.y_offset + data.height) {
        return 1;
    }
    return 0;
}

int widget_minimap_handle_mouse(const mouse *m)
{
    if ((m->left.went_down || m->right.went_down) && is_in_minimap(m)) {
        int grid_offset = get_mouse_grid_offset(m);
        if (grid_offset > 0) {
            city_view_go_to_grid_offset(grid_offset);
            widget_sidebar_invalidate_minimap();
            return 1;
        }
    }
    return 0;
}
