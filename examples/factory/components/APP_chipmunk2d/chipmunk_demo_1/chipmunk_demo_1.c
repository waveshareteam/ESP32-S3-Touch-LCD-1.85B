/**
 * @file chipmunk_game.c
 * @brief 陀螺仪五边形收集小游戏
 * @note 适配ESP32 + LVGL + QMI8658 + Chipmunk2D，屏幕280x280
 * 修复点：1. 背景固定不移动 2. 下边界可正常反弹 3. 背景全黑+260x260白框
 */
#include "chipmunk_demo_1.h"
#include "lvgl.h"
#include <chipmunk/chipmunk.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "qmi8658.h"  
#include "bsp/esp-bsp.h"

// ========================== 游戏配置（保持260x260） ==========================
#define SCREEN_WIDTH      220     // 屏幕宽度
#define SCREEN_HEIGHT     220     // 屏幕高度
#define PENTAGON_SIZE     48      // 五边形尺寸
#define PENTAGON_HALF     (PENTAGON_SIZE / 2)
#define COIN_SIZE         16      // 金币尺寸
#define COIN_HALF         (COIN_SIZE / 2)
#define GRAVITY_SCALE     120.0f  // 陀螺仪重力缩放
#define UPDATE_FPS        30      // 刷新帧率
#define COIN_SPAWN_TIME   3000    // 金币生成间隔(ms)
#define MAX_COIN_COUNT    4       // 最大同时存在金币数

// ========================== 全局变量 ==========================
// 物理相关
cpSpace *space;
cpBody *pentagonBody;
cpShape *pentagonShape;
cpBody *staticBody;
// LVGL UI相关
lv_obj_t *imgPentagon;
lv_obj_t *bg;
static lv_obj_t *game_root;
lv_obj_t *scoreLabel;
lv_obj_t *gameOverLabel;
lv_obj_t *restartBtn;
// 游戏状态
static qmi8658_dev_t *qmi8658_dev;
qmi8658_data_t gyro_data;
static int gameScore = 0;
static bool isGameOver = false;
static uint32_t lastCoinSpawnTime = 0;
// 金币对象
typedef struct {
    lv_obj_t *img;
    cpShape *shape;
    cpVect pos;
    bool isActive;
} Coin_t;
static Coin_t coins[MAX_COIN_COUNT] = {0};

// 图片资源
LV_IMG_DECLARE(pentagon);   // 五边形图片(48x48)
LV_IMG_DECLARE(coin);       // 金币图片(20x20)

// ========================== 函数声明 ==========================
static lv_coord_t lv_custom_clamp(lv_coord_t val, lv_coord_t min, lv_coord_t max);
static void reset_game(void);
static void init_physics_space(void);
static void create_pentagon_physic_body(void);
static void create_screen_border(void);
static void spawn_coin_logic(void);
static void spawn_coin_ui(int freeIdx, cpVect pos);
static void check_coin_collision(void);
static void update_gravity_from_gyro(void);
static void init_game_ui(void);
static void update_score_ui(void);
static void show_game_over(void);
static void hide_game_over(void);
static void restart_btn_cb(lv_event_t *e);
static void update_game_ui(void);
static void check_game_over(void);

static void game_tick(lv_timer_t *timer);

/**
 * @brief 自定义范围限制函数
 */
static lv_coord_t lv_custom_clamp(lv_coord_t val, lv_coord_t min, lv_coord_t max) {
    if(val < min) return min;
    if(val > max) return max;
    return val;
}

/**
 * @brief 初始化物理空间
 */
static void init_physics_space(void) {
    space = cpSpaceNew();
    if(space == NULL) {
        ESP_LOGE("PHYSICS", "创建物理空间失败！");
        return;
    }

    space->gravity = cpv(0, 150);
    space->damping = 0.95;
    space->collisionSlop = 0.5f;

    staticBody = cpSpaceGetStaticBody(space);
    create_screen_border();
}

/**
 * @brief 创建屏幕边界（核心修复：下边界恢复弹性，位置修正）
 */
static void create_screen_border(void) {
    cpShape *borderShape = NULL;

    // 左边界
    borderShape = cpSegmentShapeNew(staticBody, cpv(0, 0), cpv(0, SCREEN_HEIGHT), 2.0f);
    cpShapeSetFriction(borderShape, 1.0f);
    cpShapeSetElasticity(borderShape, 0.7f);
    cpSpaceAddShape(space, borderShape);

    // 下边界（修复1：位置改为SCREEN_HEIGHT，弹性改为0.7，和其他边界一致）
    borderShape = cpSegmentShapeNew(staticBody, cpv(0, SCREEN_HEIGHT), cpv(SCREEN_WIDTH, SCREEN_HEIGHT), 2.0f);
    cpShapeSetFriction(borderShape, 1.0f);
    cpShapeSetElasticity(borderShape, 0.7f); // 关键修复：从0.0改为0.7，恢复反弹
    cpSpaceAddShape(space, borderShape);

    // 右边界
    borderShape = cpSegmentShapeNew(staticBody, cpv(SCREEN_WIDTH, SCREEN_HEIGHT), cpv(SCREEN_WIDTH, 0), 2.0f);
    cpShapeSetFriction(borderShape, 1.0f);
    cpShapeSetElasticity(borderShape, 0.7f);
    cpSpaceAddShape(space, borderShape);

    // 上边界
    borderShape = cpSegmentShapeNew(staticBody, cpv(SCREEN_WIDTH, 0), cpv(0, 0), 2.0f);
    cpShapeSetFriction(borderShape, 1.0f);
    cpShapeSetElasticity(borderShape, 0.7f);
    cpSpaceAddShape(space, borderShape);

    ESP_LOGI("PHYSICS", "屏幕边界创建完成（下边界已修复反弹）");

    // ====================== 新增：中间一段水平墙体 ======================
    // 墙体位置：水平中线（y=130），从x=50到x=210（260×260区域的中间）
    borderShape = cpSegmentShapeNew(staticBody, cpv(SCREEN_WIDTH/2-30, SCREEN_HEIGHT/2), cpv(SCREEN_WIDTH/2+30, SCREEN_HEIGHT/2), 8.0f);
    cpShapeSetFriction(borderShape, 1.0f);    // 摩擦系数和边界一致
    cpShapeSetElasticity(borderShape, 0.7f);  // 弹性系数和边界一致（可反弹）
    cpSpaceAddShape(space, borderShape);
    // ===================================================================

    ESP_LOGI("PHYSICS", "屏幕边界创建完成（下边界已修复反弹，新增中间墙体）");
}

/**
 * @brief 创建五边形物理刚体
 */
static void create_pentagon_physic_body(void) {
    srand((unsigned int)time(NULL));

    cpFloat x = SCREEN_WIDTH / 2;
    cpFloat y = SCREEN_HEIGHT / 4;

    cpFloat mass = 1.0f;
    cpFloat moment = cpMomentForBox(mass, PENTAGON_SIZE, PENTAGON_SIZE);
    pentagonBody = cpBodyNew(mass, moment);
    if(pentagonBody == NULL) {
        ESP_LOGE("PHYSICS", "创建五边形刚体失败！");
        return;
    }
    cpBodySetPosition(pentagonBody, cpv(x, y));
    cpSpaceAddBody(space, pentagonBody);

    pentagonShape = cpBoxShapeNew(pentagonBody, PENTAGON_SIZE, PENTAGON_SIZE, 0.0f);
    cpShapeSetFriction(pentagonShape, 0.8f);
    cpShapeSetElasticity(pentagonShape, 0.7f);
    cpSpaceAddShape(space, pentagonShape);

    cpBodySetVelocity(pentagonBody, cpv(0, 0));
    cpBodySetAngularVelocity(pentagonBody, 0);
}

/**
 * @brief 生成金币逻辑
 */
static void spawn_coin_logic(void) {
    if(isGameOver) return;

    int freeIdx = -1;
    for(int i=0; i<MAX_COIN_COUNT; i++) {
        if(!coins[i].isActive) {
            freeIdx = i;
            break;
        }
    }
    if(freeIdx == -1) return;

    cpFloat x = cpflerp(COIN_SIZE+10, SCREEN_WIDTH-COIN_SIZE-10, (cpFloat)rand() / RAND_MAX);
    cpFloat y = cpflerp(COIN_SIZE+10, SCREEN_HEIGHT-COIN_SIZE-10, (cpFloat)rand() / RAND_MAX);
    cpVect coinPos = cpv(x, y);

    coins[freeIdx].shape = cpCircleShapeNew(staticBody, COIN_HALF, coinPos);
    cpShapeSetSensor(coins[freeIdx].shape, true);
    cpSpaceAddShape(space, coins[freeIdx].shape);
    coins[freeIdx].pos = coinPos;

    spawn_coin_ui(freeIdx, coinPos);
}

/**
 * @brief 生成金币UI
 */
static void spawn_coin_ui(int freeIdx, cpVect pos) {

    coins[freeIdx].img = lv_image_create(bg);
    lv_image_set_src(coins[freeIdx].img, &coin);
    lv_obj_set_size(coins[freeIdx].img, COIN_SIZE, COIN_SIZE);
    lv_obj_set_pos(coins[freeIdx].img, pos.x-COIN_HALF, pos.y-COIN_HALF);
    coins[freeIdx].isActive = true;

}

/**
 * @brief 检查五边形与金币的碰撞
 */
static void check_coin_collision(void) {
    if(isGameOver) return;

    cpVect pentagonPos = cpBodyGetPosition(pentagonBody);
    for(int i=0; i<MAX_COIN_COUNT; i++) {
        if(!coins[i].isActive) continue;

        cpFloat dist = cpvdist(pentagonPos, coins[i].pos);
        if(dist < (PENTAGON_HALF + COIN_HALF)) {
            cpSpaceRemoveShape(space, coins[i].shape);
            cpShapeFree(coins[i].shape);


            lv_obj_del(coins[i].img);


            gameScore += 10;
            update_score_ui();

            coins[i].isActive = false;
        }
    }
}

/**
 * @brief 检查游戏结束（仅：分数达到100分即结束游戏）
 */
static void check_game_over(void) {
    if(isGameOver) return;

    // 唯一判定条件：分数≥100分 结束游戏
    if(gameScore >= 100) {
        isGameOver = true;
        show_game_over();
    }
}

/**
 * @brief 更新陀螺仪重力
 */
static void update_gravity_from_gyro(void) {
    if(isGameOver) return;

    int ret = 0;
    ret = qmi8658_read_sensor_data(qmi8658_dev, &gyro_data);
    if(ret != 0) {
        ESP_LOGE("GYRO", "读取陀螺仪数据失败！ret=%d", ret);
        return;
    }

    cpFloat gravity_x = -gyro_data.accelX * GRAVITY_SCALE;
    cpFloat gravity_y = gyro_data.accelY * GRAVITY_SCALE;
    cpSpaceSetGravity(space, cpv(gravity_x, gravity_y));
}

/**
 * @brief 初始化游戏UI（核心修复：背景固定+全黑+白框）
 */
static void init_game_ui(void) {

    
    lv_obj_t *pa = game_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(pa, 360, 360);
    lv_obj_align(pa, LV_ALIGN_CENTER, 0, 0); 
    lv_obj_set_style_bg_color(pa, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT); // 纯黑背景
    lv_obj_set_style_bg_opa(pa, LV_OPA_100, LV_PART_MAIN | LV_STATE_DEFAULT); // 背景完全不透明
    lv_obj_set_style_border_width(pa, 0, LV_PART_MAIN | LV_STATE_DEFAULT); // 强制关闭边框（核心：去掉默认边框白边）
    lv_obj_set_style_outline_width(pa, 0, LV_PART_MAIN | LV_STATE_DEFAULT); // 关闭外轮廓
    lv_obj_set_style_shadow_width(pa, 0, LV_PART_MAIN | LV_STATE_DEFAULT); // 关闭阴影
    lv_obj_set_style_pad_all(pa, 0, LV_PART_MAIN | LV_STATE_DEFAULT); // 关闭内边距
    lv_obj_set_style_margin_all(pa, 0, LV_PART_MAIN | LV_STATE_DEFAULT); // 关闭外边距

    lv_obj_set_scrollbar_mode(pa, LV_SCROLLBAR_MODE_OFF); 
    lv_obj_set_scroll_dir(pa, LV_DIR_NONE);

    // 1. 背景容器（修复2：固定左上角位置，防止移动；全黑背景+白框）
    bg = lv_obj_create(pa);
    lv_obj_set_size(bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 26); // 关键修复：从center改为top-left，固定位置
    lv_obj_set_style_pad_all(bg, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bg, lv_color_hex(0x000000), LV_PART_MAIN); // 背景全黑
    lv_obj_set_style_border_width(bg, 2, LV_PART_MAIN); // 2像素白框
    lv_obj_set_style_border_color(bg, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // 白色边框
    lv_obj_set_style_border_opa(bg, LV_OPA_100, LV_PART_MAIN); // 边框不透明
    lv_obj_set_style_radius(bg, 12, LV_PART_MAIN); // 无圆角
    lv_obj_set_scrollbar_mode(bg, LV_SCROLLBAR_MODE_OFF); 
    lv_obj_set_scroll_dir(bg,LV_DIR_NONE);

    // ===== 2. LVGL视觉墙（坐标/宽度和物理墙一致） =====

    static lv_point_precise_t line_points[] = { {SCREEN_WIDTH/2-30, SCREEN_HEIGHT/2}, {SCREEN_WIDTH/2+30, SCREEN_HEIGHT/2} };
    lv_obj_t *line1 = lv_line_create(bg);
    lv_line_set_points(line1, line_points, 2);
    lv_obj_set_style_line_width(line1,8,0);
    lv_obj_set_style_line_color(line1,lv_color_hex(0x0000ff),0);
    lv_obj_set_style_line_rounded(line1,true,0);
    lv_obj_set_pos(line1, 0, 0); // 点坐标直接相对bg的(0,0)



    // 2. 五边形UI
    imgPentagon = lv_image_create(bg);
    lv_image_set_src(imgPentagon, &pentagon);
    lv_obj_set_size(imgPentagon, PENTAGON_SIZE, PENTAGON_SIZE);

    // 3. 分数标签
    scoreLabel = lv_label_create(bg);
    lv_label_set_text(scoreLabel, "score:0");
    lv_obj_set_style_text_color(scoreLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(scoreLabel, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(scoreLabel, LV_ALIGN_TOP_LEFT, 10, 10);

    // 4. 游戏结束标签
    gameOverLabel = lv_label_create(bg);
    lv_label_set_text(gameOverLabel, "game over\nscore:0");
    lv_obj_set_style_text_color(gameOverLabel, lv_color_hex(0x00FF11), LV_PART_MAIN);
    lv_obj_set_style_text_font(gameOverLabel, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(gameOverLabel, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_flag(gameOverLabel, LV_OBJ_FLAG_HIDDEN);

    // 5. 重启按钮
    restartBtn = lv_btn_create(bg);
    lv_obj_set_size(restartBtn, 100, 40);
    lv_obj_align(restartBtn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_flag(restartBtn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(restartBtn, restart_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *restartLabel = lv_label_create(restartBtn);
    lv_label_set_text(restartLabel, "restart");
    lv_obj_center(restartLabel);


}

/**
 * @brief 更新分数显示
 */
static void update_score_ui(void) {
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "score:%d", gameScore);
    

    lv_label_set_text(scoreLabel, scoreText);

}

/**
 * @brief 显示游戏结束界面
 */
static void show_game_over(void) {
    char gameOverText[64];
    // 根据分数判断是获胜还是失败（如果保留了下边界判定）
    if(gameScore >= 100) {
        snprintf(gameOverText, sizeof(gameOverText), "Game victory! \n Final score:%d", gameScore);
    } else {
        snprintf(gameOverText, sizeof(gameOverText), "Game failed! \n Final score:%d", gameScore);
    }
    

    lv_label_set_text(gameOverLabel, gameOverText);
    lv_obj_remove_flag(gameOverLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(restartBtn, LV_OBJ_FLAG_HIDDEN);

}

/**
 * @brief 隐藏游戏结束界面
 */
static void hide_game_over(void) {

    lv_obj_add_flag(gameOverLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(restartBtn, LV_OBJ_FLAG_HIDDEN);

}

/**
 * @brief 重启按钮回调
 */
static void restart_btn_cb(lv_event_t *e) {
    reset_game();
}

/**
 * @brief 重置游戏状态
 */
static void reset_game(void) {
    gameScore = 0;
    isGameOver = false;
    lastCoinSpawnTime = lv_tick_get();

    cpBodySetPosition(pentagonBody, cpv(SCREEN_WIDTH/2, SCREEN_HEIGHT/4));
    cpBodySetVelocity(pentagonBody, cpv(0, 0));
    cpBodySetAngularVelocity(pentagonBody, 0);
    cpBodySetAngle(pentagonBody, 0);

    for(int i=0; i<MAX_COIN_COUNT; i++) {
        if(coins[i].isActive) {
            cpSpaceRemoveShape(space, coins[i].shape);
            cpShapeFree(coins[i].shape);
            

            lv_obj_delete(coins[i].img);

            
            coins[i].isActive = false;
        }
    }

    update_score_ui();
    hide_game_over();
    spawn_coin_logic();
}

/**
 * @brief 统一更新游戏UI
 */
static void update_game_ui(void) {
    if(isGameOver) return;

    cpVect pos = cpBodyGetPosition(pentagonBody);
    cpFloat angle_rad = cpBodyGetAngle(pentagonBody);
    cpFloat angle_deg = angle_rad * 180 / M_PI;

    lv_coord_t lv_x = pos.x - PENTAGON_HALF;
    lv_coord_t lv_y = pos.y - PENTAGON_HALF;
    lv_x = lv_custom_clamp(lv_x, 0, SCREEN_WIDTH - PENTAGON_SIZE);
    // 增加Y轴限制，防止五边形超出可视区域
    lv_y = lv_custom_clamp(lv_y, 0, SCREEN_HEIGHT - PENTAGON_SIZE);


    lv_obj_set_pos(imgPentagon, lv_x, lv_y);
    lv_image_set_rotation(imgPentagon, angle_deg);

}

// ========================== 全局状态变量（暂停/恢复） ==========================
static bool isGamePaused = false;
static lv_timer_t *game_timer = NULL;

// ========================== 外部调用接口 ==========================
void chipmunk_game_pause(void) {
    if(isGameOver || isGamePaused) return;
    isGamePaused = true;
}

void chipmunk_game_resume(void) {
    if(isGameOver || !isGamePaused) return;
    isGamePaused = false;
    lastCoinSpawnTime = lv_tick_get();
}

static void remove_shape_after_iteration(cpSpace *owner,void *key,void *unused) {
    (void)unused;
    cpSpaceRemoveShape(owner,(cpShape *)key);
    cpShapeFree((cpShape *)key);
}
static void remove_body_after_iteration(cpSpace *owner,void *key,void *unused) {
    (void)unused;
    cpSpaceRemoveBody(owner,(cpBody *)key);
    cpBodyFree((cpBody *)key);
}
static void free_shape(cpShape *shape,void *owner) {
    cpSpaceAddPostStepCallback((cpSpace *)owner,remove_shape_after_iteration,shape,NULL);
}
static void free_body(cpBody *body,void *owner) {
    cpSpaceAddPostStepCallback((cpSpace *)owner,remove_body_after_iteration,body,NULL);
}
void chipmunk_game_deinit(void) {
    if(game_timer) { lv_timer_delete(game_timer); game_timer=NULL; }
    if(space) {
        // Iterators lock the space. Remove objects after each iteration ends,
        // so body/shape lists are empty before cpSpaceFree walks them again.
        cpSpaceEachShape(space,free_shape,space);
        cpSpaceEachBody(space,free_body,space);
        cpSpaceFree(space); space=NULL;
    }
    pentagonShape=NULL; pentagonBody=NULL; staticBody=NULL;
    if(game_root) { lv_obj_delete(game_root); game_root=NULL; }
    imgPentagon=scoreLabel=gameOverLabel=restartBtn=bg=NULL;
    memset(coins,0,sizeof(coins)); gameScore=0; lastCoinSpawnTime=0;
    qmi8658_dev=NULL; isGameOver=false; isGamePaused=false;
}

void start_chipmunk_esp_example_1(qmi8658_dev_t *device) {
    chipmunk_game_deinit();
    if(!device) return;
    qmi8658_dev=device; init_game_ui(); init_physics_space();
    if(!space) { chipmunk_game_deinit(); return; }
    create_pentagon_physic_body();
    if(!pentagonBody) { chipmunk_game_deinit(); return; }
    lastCoinSpawnTime=lv_tick_get(); spawn_coin_logic();
    game_timer=lv_timer_create(game_tick,33,NULL);
    if(!game_timer) chipmunk_game_deinit();
}

// ========================== 游戏主循环 ==========================
static void game_tick(lv_timer_t *timer) {
    (void)timer;
    if(isGamePaused || isGameOver || !space) return;
    update_gravity_from_gyro(); cpSpaceStep(space,1.0f/UPDATE_FPS);
    uint32_t now=lv_tick_get();
    if(now-lastCoinSpawnTime>COIN_SPAWN_TIME) { spawn_coin_logic(); lastCoinSpawnTime=now; }
    check_coin_collision(); check_game_over(); update_game_ui();
}
