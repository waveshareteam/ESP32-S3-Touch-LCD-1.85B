#include "chipmunk_demo.h"
#include "lvgl.h"
#include <chipmunk/chipmunk.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>  // 引入math.h以使用M_PI
// 引入QMI8658相关头文件（其中已定义qmi8658_data_t）
#include "qmi8658.h"  

// 屏幕与物理世界参数（360*360屏幕，中间正方形背景）
#define SCREEN_SIZE 360           // 屏幕总尺寸（360*360）
#define SQUARE_SIZE 260           // 中间正方形背景尺寸
#define BASE_GRAVITY 150          // 基础重力值
#define DAMPING 0.95              // 阻尼系数
#define BLOCK_COUNT 5             // 方块数量
#define MIN_BLOCK_SIZE 20         // 方块最小尺寸
#define MAX_BLOCK_SIZE 60         // 方块最大尺寸
#define BORDER_WIDTH 2            // 正方形边框宽度
#define GYRO_SENSITIVITY 0.005f   // 陀螺仪旋转灵敏度
#define ACCEL_SENSITIVITY 0.5f    // 加速度计重力感应灵敏度

// 方块数据结构（新增独立样式成员）
typedef struct {
    cpBody *body;                 // 物理刚体
    cpShape *shape;               // 碰撞形状
    lv_obj_t *obj;                // LVGL视觉对象
    lv_style_t style;             // 方块独立样式（核心新增）
    lv_coord_t size;              // 方块尺寸
} Block_t;

// 物理空间与边界
static cpSpace *space;
static cpBody *staticBody;               // 静态刚体（边界）
static cpShape *boundaryShapes[4];       // 正方形物理边界的4条边

// LVGL 层级对象
static lv_obj_t *demo_container;         // 最底层全屏容器
static lv_obj_t *square_container;       // 中间正方形背景容器
static lv_style_t container_style;       // 容器通用样式
static lv_style_t square_style;          // 正方形容器样式

// 拖动相关全局变量
static Block_t *drag_block = NULL;       // 当前被拖动的方块
static lv_point_t drag_start_pos;        // 拖动开始时的触摸点
static cpVect drag_start_body_pos;       // 拖动开始时的方块物理位置
static cpVect current_gravity;          // 当前重力向量（由陀螺仪动态调整）

// 方块数组
Block_t blocks[BLOCK_COUNT];      // 方块数组
static lv_timer_t *timer  = NULL;
// QMI8658设备句柄（假设已在其他地方初始化）
qmi8658_dev_t *qmi8658_dev; 

// 函数声明
static void on_timer(lv_timer_t *t);
void init_physical_boundary(void);
void create_block(uint8_t index);
void clamp_block_position(Block_t *block);
void block_touch_event_cb(lv_event_t *e);
void convert_point_to_local(lv_obj_t *obj, lv_point_t *in, lv_point_t *out);

/**
 * @brief 初始化物理空间
 */
void init_physical_space(void) {
    srand((unsigned int)time(NULL));
    space = cpSpaceNew();
    cpSpaceSetCollisionSlop(space, 0.0f); // 碰撞容差设为0，禁止穿透
    cpSpaceSetIterations(space, 100);    // 提高迭代次数，增强碰撞精度
    current_gravity = cpv(0, BASE_GRAVITY); // 初始重力向下
    cpSpaceSetGravity(space, current_gravity);
    cpSpaceSetDamping(space, DAMPING);
    staticBody = cpSpaceGetStaticBody(space);
    init_physical_boundary();
}

/**
 * @brief 创建正方形物理边界
 */
void init_physical_boundary(void) {
    // 正方形物理边界的坐标：与视觉边框完全对齐（无BORDER_WIDTH偏移）
    cpFloat square_x = (SCREEN_SIZE - SQUARE_SIZE) / 2; // 正方形容器X坐标
    cpFloat square_y = (SCREEN_SIZE - SQUARE_SIZE) / 2; // 正方形容器Y坐标
    // 物理边界与视觉容器的边框完全重合（移除BORDER_WIDTH偏移）
    cpFloat left = square_x;
    cpFloat right = square_x + SQUARE_SIZE;
    cpFloat top = square_y;
    cpFloat bottom = square_y + SQUARE_SIZE;
    cpFloat thickness = 0.0f; // 设为0，线段无厚度，避免碰撞体偏移

    // 创建4条边的物理边界（修正线段方向，确保法线朝向容器内部）
    // 左边界：从(top, left)到(bottom, left)，法线朝右
    boundaryShapes[0] = cpSegmentShapeNew(staticBody, cpv(left, top), cpv(left, bottom), thickness);
    // 右边界：从(top, right)到(bottom, right)，法线朝左
    boundaryShapes[1] = cpSegmentShapeNew(staticBody, cpv(right, top), cpv(right, bottom), thickness);
    // 上边界：从(left, top)到(right, top)，法线朝下
    boundaryShapes[2] = cpSegmentShapeNew(staticBody, cpv(left, top), cpv(right, top), thickness);
    // 下边界：从(left, bottom)到(right, bottom)，法线朝上
    boundaryShapes[3] = cpSegmentShapeNew(staticBody, cpv(left, bottom), cpv(right, bottom), thickness);

    // 设置边界属性（启用碰撞检测，高弹性/摩擦力）
    for (int i = 0; i < 4; i++) {
        cpShapeSetElasticity(boundaryShapes[i], 0.8f);
        cpShapeSetFriction(boundaryShapes[i], 0.6f);
        cpShapeSetCollisionType(boundaryShapes[i], 1); // 标记为边界碰撞类型
        cpSpaceAddShape(space, boundaryShapes[i]);
    }
}

/**
 * @brief 转换触摸坐标为正方形容器内局部坐标
 */
void convert_point_to_local(lv_obj_t *obj, lv_point_t *in, lv_point_t *out) {
    out->x = in->x - lv_obj_get_x(obj);
    out->y = in->y - lv_obj_get_y(obj);
}

/**
 * @brief 方块位置硬限制（仅做安全兜底，不干扰物理碰撞）
 */
void clamp_block_position(Block_t *block) {
    cpVect pos = cpBodyGetPosition(block->body);
    lv_coord_t half_size = block->size / 2;
    lv_coord_t square_x = (SCREEN_SIZE - SQUARE_SIZE) / 2;
    lv_coord_t square_y = (SCREEN_SIZE - SQUARE_SIZE) / 2;

    // 物理边界的实际范围（与init_physical_boundary一致）
    cpFloat min_x = square_x + half_size;    // 方块中心最小X（避免左边界穿透）
    cpFloat max_x = square_x + SQUARE_SIZE - half_size; // 方块中心最大X
    cpFloat min_y = square_y + half_size;    // 方块中心最小Y（避免上边界穿透）
    cpFloat max_y = square_y + SQUARE_SIZE - half_size; // 方块中心最大Y

    // 仅当方块中心超出范围时，做兜底修正（软限制，保留速度）
    cpFloat new_x = pos.x;
    cpFloat new_y = pos.y;
    if (pos.x < min_x) new_x = min_x;
    if (pos.x > max_x) new_x = max_x;
    if (pos.y < min_y) new_y = min_y;
    if (pos.y > max_y) new_y = max_y;

    // 仅在位置变化时更新（避免频繁修改干扰物理引擎）
    if (new_x != pos.x || new_y != pos.y) {
        cpBodySetPosition(block->body, cpv(new_x, new_y));
        // 轻微衰减速度，避免反弹抖动
        cpBodySetVelocity(block->body, cpv(cpBodyGetVelocity(block->body).x * 0.9f, cpBodyGetVelocity(block->body).y * 0.9f));
    }

    // 同步视觉位置（物理坐标 -> 视觉坐标，无偏移）
    lv_obj_set_pos(block->obj, 
                   (lv_coord_t)(pos.x - square_x - half_size), 
                   (lv_coord_t)(pos.y - square_y - half_size));
    
    // 设置旋转角度
    float rotation_angle = cpBodyGetAngle(block->body) * 180 / M_PI;
    lv_style_set_transform_rotation(&block->style, rotation_angle);
}

/**
 * @brief 方块触摸事件回调
 */
void block_touch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    Block_t *block = NULL;

    // 找到触摸的方块
    for (int i = 0; i < BLOCK_COUNT; i++) {
        if (blocks[i].obj == obj) {
            block = &blocks[i];
            break;
        }
    }
    if (block == NULL) return;

    // 获取触摸坐标
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t touch_pos;
    lv_indev_get_point(indev, &touch_pos);
    convert_point_to_local(square_container, &touch_pos, &touch_pos);

    // 触摸按下
    if (code == LV_EVENT_PRESSED) {
        drag_block = block;
        drag_start_pos = touch_pos;
        drag_start_body_pos = cpBodyGetPosition(block->body);
        cpSpaceSetGravity(space, cpv(0, 0)); // 关闭重力
        cpBodySetVelocity(block->body, cpv(0, 0));
        cpBodySetAngularVelocity(block->body, 0.0f);
        lv_obj_move_foreground(obj);
    }
    // 触摸移动
    else if (code == LV_EVENT_PRESSING && drag_block == block) {
        cpFloat dx = touch_pos.x - drag_start_pos.x;
        cpFloat dy = touch_pos.y - drag_start_pos.y;
        cpFloat new_x = drag_start_body_pos.x + dx;
        cpFloat new_y = drag_start_body_pos.y + dy;
        cpBodySetPosition(block->body, cpv(new_x, new_y));
        clamp_block_position(block); // 实时硬限制
    }
    // 触摸释放
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (drag_block == block) {
            cpSpaceSetGravity(space, current_gravity); // 恢复陀螺仪控制的重力
            drag_block = NULL;
        }
    }
}

/**
 * @brief 创建单个方块（核心修改：初始化独立样式并绑定）
 */
void create_block(uint8_t index) {
    if (index >= BLOCK_COUNT) return;

    Block_t *block = &blocks[index];
    block->size = MIN_BLOCK_SIZE + rand() % (MAX_BLOCK_SIZE - MIN_BLOCK_SIZE + 1);
    cpFloat half_size = block->size / 2.0f;
    cpFloat square_x = (SCREEN_SIZE - SQUARE_SIZE) / 2;
    cpFloat square_y = (SCREEN_SIZE - SQUARE_SIZE) / 2;
    // 初始位置：确保方块完全在物理边界内（移除half_size+10的偏移）
    lv_coord_t visual_left = square_x + half_size + 5;  // 仅留5px边距
    lv_coord_t visual_right = square_x + SQUARE_SIZE - half_size - 5;
    lv_coord_t visual_top = square_y + half_size + 5;
    lv_coord_t visual_bottom = square_y + SQUARE_SIZE/2 - half_size - 5;


    cpFloat x = visual_left + rand() % (visual_right - visual_left);
    cpFloat y = visual_top + rand() % (visual_bottom - visual_top);

    // 1. 创建物理刚体（设置质量，增加惯性）
    cpFloat mass = 1.0f;
    cpFloat moment = cpMomentForBox(mass, block->size, block->size);
    block->body = cpBodyNew(mass, moment);
    cpBodySetPosition(block->body, cpv(x, y));
    cpBodySetMass(block->body, mass);
    cpSpaceAddBody(space, block->body);

    // 2. 创建碰撞形状（匹配方块尺寸）
    block->shape = cpBoxShapeNew(block->body, block->size, block->size, 0.0f);
    cpShapeSetElasticity(block->shape, 0.8f);
    cpShapeSetFriction(block->shape, 0.6f);
    cpSpaceAddShape(space, block->shape);

    // 3. 创建LVGL视觉对象
    block->obj = lv_obj_create(square_container);
    lv_obj_remove_style_all(block->obj); // 移除默认样式，避免冲突

    // 初始化方块独立样式（核心新增）
    lv_style_init(&block->style);

    // 设置样式基础属性
    lv_color_t color = lv_color_make(rand() % 200 + 55, rand() % 200 + 55, rand() % 200 + 55);
    lv_style_set_bg_color(&block->style, color);          // 背景色
    lv_style_set_bg_opa(&block->style, LV_OPA_100);       // 背景不透明度
    lv_style_set_border_color(&block->style, lv_color_black()); // 边框色
    lv_style_set_border_width(&block->style, 1);          // 边框宽度

    // 将样式绑定到LVGL对象（核心步骤，确保样式生效）
    lv_obj_add_style(block->obj, &block->style, LV_PART_MAIN);

    // 设置对象尺寸和位置
    lv_obj_set_size(block->obj, block->size, block->size);
    lv_obj_set_pos(block->obj, x - square_x - half_size, y - square_y - half_size);

    // 4. 绑定触摸事件
    lv_obj_add_event_cb(block->obj, block_touch_event_cb, LV_EVENT_ALL, NULL);

    // 5. 随机初始速度（低速，减少穿透风险）
    cpFloat vx = (rand() % 80) - 40;
    cpFloat vy = (rand() % 40) - 20;
    cpBodySetVelocity(block->body, cpv(vx, vy));
}

/**
 * @brief 定时器回调（物理模拟+陀螺仪数据读取）
 */
static void on_timer(lv_timer_t *t) {
    LV_UNUSED(t);
    qmi8658_data_t gyro_data; // 使用头文件中定义的结构体
    int ret;

    // 1. 读取陀螺仪数据（直接调用接口，无需初始化）
    ret = qmi8658_read_sensor_data(qmi8658_dev, &gyro_data);
    if (ret == 0) {
        // 1.1 根据加速度计数据调整重力方向（拖动时不调整）
        if (drag_block == NULL) {
            current_gravity.x = -gyro_data.accelX * ACCEL_SENSITIVITY * BASE_GRAVITY;
            current_gravity.y = gyro_data.accelY * ACCEL_SENSITIVITY * BASE_GRAVITY;
            cpSpaceSetGravity(space, current_gravity);
        }

        // 1.2 根据陀螺仪Z轴数据控制方块旋转（非拖动状态）
        for (int i = 0; i < BLOCK_COUNT; i++) {
            Block_t *block = &blocks[i];
            if (block == drag_block) continue;

            // 累加陀螺仪角速度到方块旋转角速度
            cpFloat angular_vel = cpBodyGetAngularVelocity(block->body);
            angular_vel += gyro_data.gyroZ * GYRO_SENSITIVITY;
            cpBodySetAngularVelocity(block->body, angular_vel);
        }
    }

    // 2. 分步执行物理模拟，提高碰撞精度
    for (int i = 0; i < 2; i++) {
        cpSpaceStep(space, 1.0f / 60.0f);
    }

    // 3. 对所有方块进行硬限制和视觉同步
    for (int i = 0; i < BLOCK_COUNT; i++) {
        clamp_block_position(&blocks[i]);
    }
}

/**
 * @brief 启动方块Demo（新增容器样式初始化）
 */
void start_chipmunk_esp_example(qmi8658_dev_t *qmi8658) 
{
    qmi8658_dev = qmi8658;
    // 初始化容器样式
    lv_style_init(&container_style);
    lv_style_set_bg_color(&container_style, lv_color_hex(0x121212));
    lv_style_init(&square_style);
    lv_style_set_bg_color(&square_style, lv_color_hex(0x333333));
    lv_style_set_border_color(&square_style, lv_color_white());
    lv_style_set_border_width(&square_style, BORDER_WIDTH);

    // 1. 全屏容器（黑色背景）
    
    demo_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(demo_container, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_center(demo_container);
    lv_obj_clear_flag(demo_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(demo_container, lv_color_hex(0x0), LV_PART_MAIN);  // 全局浅灰
    lv_obj_set_style_bg_opa(demo_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(demo_container, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(demo_container, 0, LV_PART_MAIN);
    
    // 2. 正方形背景容器（视觉边框）
    square_container = lv_obj_create(demo_container);
    lv_obj_set_size(square_container, SQUARE_SIZE, SQUARE_SIZE);
    lv_obj_center(square_container);
    lv_obj_add_style(square_container, &square_style, LV_PART_MAIN);
    lv_obj_clear_flag(square_container, LV_OBJ_FLAG_SCROLLABLE);

    // 3. 初始化物理空间
    init_physical_space();

    // 4. 创建方块
    for (int i = 0; i < BLOCK_COUNT; i++) {
        create_block(i);
    }

    // 5. 创建高精度定时器
    timer = lv_timer_create(on_timer, 16, NULL); // ~60FPS
    lv_timer_set_repeat_count(timer, -1);
}

/**
 * @brief 恢复完整的资源释放函数（新增样式销毁）
 */
void stop_block_demo(void) 
{
    if (timer)
    {
        lv_timer_del(timer);
        timer = NULL;
    }
    for (int i = 0; i < BLOCK_COUNT; i++) {
        if (blocks[i].body) cpSpaceRemoveBody(space, blocks[i].body);
        if (blocks[i].shape) cpSpaceRemoveShape(space, blocks[i].shape);
        if (blocks[i].obj) lv_obj_del(blocks[i].obj);
        //lv_style_del(&blocks[i].style); // 销毁方块独立样式
        if (blocks[i].body) cpBodyFree(blocks[i].body);
        if (blocks[i].shape) cpShapeFree(blocks[i].shape);
    }

    // 销毁容器样式
    // lv_style_del(&container_style);
    // lv_style_del(&square_style);

    for (int i = 0; i < 4; i++) {
        if (boundaryShapes[i]) cpSpaceRemoveShape(space, boundaryShapes[i]);
        if (boundaryShapes[i]) cpShapeFree(boundaryShapes[i]);
    }

    if (square_container) lv_obj_del(square_container);
    if (demo_container) lv_obj_del(demo_container);

    if (space) cpSpaceFree(space);
    space = NULL;
}