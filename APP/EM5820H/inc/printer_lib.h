#ifndef __PRINTER_LIB_H__
#define __PRINTER_LIB_H__
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "printer_type.h"
#ifdef PRINTER_BUILD_SHARED
#  define PRINTER_API __declspec(dllexport)
#else
#  define PRINTER_API
#endif
#ifdef __cplusplus
    extern "C" {
#endif
typedef struct usb_port{

    struct usb_port * (*parm)(int vid,int pid);
    /**
     * 初始化usb接口,使用此函数后打开打印机usb接口
     * @return 0:成功；其他值:失败
     */
    int (*init)();
    /* 
     * 在不使用打印机后，请调用此函数，该函数只需调用一次
     */
    void (*deinit)(void);
        /**
     * USB设备的pid参数，内部使用
     */
    uint16_t vid;
    /**
     * USB设备的pid参数，内部使用
     */
    uint16_t pid;
    /**
     * usb接口参数设置函数
     * @param vid usb的vid参数，用户提供
     * @param pid usb的pid参数，用户提供
     */
}usb_port_t;
typedef struct uart_port{

    /**
     * 串口接口参数设置函数
     * @param name 串口名称字符串，用户提供
     * @param badurate 串口波特率，用户提供
     */
    struct uart_port * (*parm)(char * name,int badurate);
    /**
     * 初始化串口接口,使用此函数后打开打印机串口接口
     * @return 0:成功；其他值:失败
     */
    int (*init)();
    /* 
     * 在不使用打印机后，请调用此函数，该函数只需调用一次
     */
    void (*deinit)(void);
    /**
     * 串口名称,内部使用
     */
    char path[256];
    /**
     * 串口波特率，内部使用
     */
    int  badurate;
}uart_port_t;
typedef struct device{
    /**
     * usb操作句柄函数，调用此函数后设置usb
     */
    struct usb_port * (*usb)(void);
    /**
     * 串口操作句柄函数，调用此函数后设置串口
     */
    struct uart_port * (*uart)(void);
    /**
     * 自动处理打印机返回数据功能开关
     * 需要对打印机放回数据自定义处理时可以设置关闭，关闭后不对打印机返回数据处理，
     * 需自行从buffer中拿取打印机返回数据，
     * 默认开启自动返回功能
     * 关闭后setting和listener部分功能会收到影响
     * @param enable DISABLE: 关闭自动处理返回  ENABLE:开启自动返回处理(默认)
     * @return 0:成功；其他值:失败
     */
    int (*auto_return)(uint8_t enable);

}device_t;

typedef struct buffer_t{
    /*
     * 功能:清空发送缓冲区
     */
    void (*clean_send)(void);
    /**
     * 功能:获取发送缓冲区
     * @param data_buffer  数据缓冲区
     * 返回值:发送缓冲数据长度，没有数据时返回0
     * 注意:请确保缓冲区有足够的大小
     */
    int (*get_send)(uint8_t* data_buffer);
}buffer_t;

typedef struct raw{
    /**
     * 功能:原生发送接口，调用此接口可以发送自定义数据到打印机
     * @param buffer  数据缓冲区
     * @param len     数据长度
     * @param timeout 超时时间
     * 返回值:发送成功时返回发送数据长度，失败时返回-1
     */
    int (*send)(uint8_t * buffer,int len,int timeout);
    /**
     * 功能:原生发送接口，调用此接口可以发送自定义数据到打印机
     * @param buffer  数据接收缓冲区
     * @param len     接收缓冲区长度
     * @param timeout 超时时间
     * 返回值:接收成功时返回接收数据长度，失败时返回-1
     * 注意:此功能受auto_return功能影响，当auto_return功能开启时，
     * 调用此函数可能会导致异常，如需正常使用，请不要开启auto_return
     */
    int (*recv)(uint8_t * buffer,int len,int timeout);
    /**
     * 功能:清空打印机接收区
     */
    void (*clear_recv)(void);
}raw_t;
typedef struct text
{
    /**
     * 功能:设置字符的行间距
     * @param space   行间距的大小，表示每行之间的间距，单位为字符或点数，取值范围（0-255）
     */
    struct text * (*line_space)(uint8_t space);

    /**
     * 功能:设置字符的右间距
     * @param space   右间距的大小，以字符为单位，取值范围为（0-255）
     */
    struct text * (*right_space)(uint8_t space);

    /**
     * 功能:移动打印位置到下一个水平定位点的位置
     */
    struct text * (*next_ht)(void);

    /**
     * 功能:将当前位置设置到距离行首 [pos × 横向或纵向移动单位] 处
     * @param pos  距离行首的绝对位置，取值范围（0-384）
     */
    struct text * (*abs_pos)(uint16_t pos);

    /**
     * 功能:将打印位置设置到距当前位置 [pos × 横向或纵向移动单位] 处
     * @param pos  距离当前位置的相对位置，取值范围（0-384）
     */
    struct text * (*rel_pos)(uint16_t pos);

    /**
     * 功能:根据传入的字节数组 'pos' 设置打印机的水平定位位置。最多支持 32 个字节的位置参数
     * @param pos   位置数组，最大长度为 32 字节。该数组中的每个字节表示一个具体的定位位置
     * @param size  pos数组的长度，最大32字节
     */
    struct text * (*ht_pos)(uint8_t* pos, uint8_t size);

    /**
     * 功能:设置文本对齐方式
     * @param type  对齐方式，取值参考'align_type_t'枚举，可选 左对齐，居中对齐，右对齐
     */
    struct text * (*align)(align_type_t type);

    /**
     * 功能:设置左边距
     * @param left_margin  左边距的大小，取值范围为（0-384），单位为 0.125 毫米
     */
    struct text * (*left_margin)(uint16_t left_margin);

    /**
     * 功能:设置横向和纵向移动单位，分别将横向移动单位近似设置成 25.4/ x mm（ 1/ x 英寸）纵向移动单位设置成 25.4/ y mm（1/ y 英寸）
     * @param x_uint  横向移动单位，表示每次横向移动的单位，取值范围（0-255）
     * @param y_uint  纵向移动单位，表示每次纵向移动的单位，取值范围（0-255）
     */
    struct text * (*move_unit)(uint8_t x_uint, uint8_t y_uint);

    /**
     * 功能:设置字符倍宽打印，使打印的字符在水平方向上变为原来的两倍宽度
     * @param double_width  是否启用倍宽打印  ENABLE:开启倍宽打印, DISABLE:关闭倍宽打印
     */
    struct text * (*double_width)(uint8_t mode);

    /**
     * 功能:设置打印文本的下划线模式
     * @param underline  下划线模式，取值参考'underline_type_t'枚举，可选 无下划线，一点宽，两点宽
     */
    struct text * (*underline)(underline_type_t underline);

    /**
     * 功能:启用或禁用加粗模式
     * @param bold  是否启用加粗            ENABLE:启用, DISABLE:禁用
     */
    struct text * (*bold)(uint8_t mode);

    /**
     * 功能:选择/取消顺时针旋转 90°
     * @param rotate_90  是否启用旋转       ENABLE:启用, DISABLE:禁用
     */
    struct text * (*rotate_90)(uint8_t mode);

    /** 
     * 功能:选择/取消180 度旋转打印
     * @param rotate_180  是否启用旋转      ENABLE:启用, DISABLE:禁用
     */
    struct text * (*rotate_180)(uint8_t mode);

    /**
     * 功能:设置黑白反显模式。反显模式下，打印机将反转打印的颜色（黑变白，白变黑）
     * @param inversion_mode 是否启用反显   ENABLE:启用, DISABLE:禁用
     */
    struct text * (*inversion)(uint8_t inversion_mode);

    /**
     * 功能:设置字符的大小，控制字符的宽度和高度
     * @param muti_width  字符宽度的倍数，取值范围为（1-8）
     * @param muti_height 字符高度的倍数，取值范围为（1-2）
     */
    struct text * (*font_size)(uint8_t muti_width, uint8_t muti_height);

    /**
     * 功能:设置打印机的打印模式
     * @param double_width     文本宽度模式   ENABLE:启用倍宽                  DIAABLE:禁用倍宽
     * @param double_height    文本高度模式   ENABLE:启用倍高                  DISABLE:禁用倍高
     * @param blod             加粗模式       ENABLE:启用加粗                  DISABLE:禁用加粗
     * @param font_type        字体类型       ENABLE:启用扩展字体（9*17）      DISABLE:禁用扩展字体(12 × 24)
     * @param underline        下划线         ENABLE:启用下划线                DISABLE:禁用下划线
     */
    struct text * (*print_mode)(uint8_t double_width, uint8_t double_height, uint8_t blod, uint8_t font_type, uint8_t underline);

    /**
     * 功能:设置打印机的汉字模式
     * @param double_width     文本宽度模式   ENABLE:启用倍宽                  DISABLE:禁用倍宽
     * @param double_height    文本高度模式   ENABLE:启用倍高                  DISABLE:禁用倍高
     * @param underline        下划线         ENABLE:启用下划线                DISABLE:禁用下划线
     */
    struct text * (*chinese_mode)(uint8_t double_width, uint8_t double_height, uint8_t underline);

    /**
     * 功能:设置字符编码页
     * @param type    字符编码，取值参考'encoding_type_t'枚举，可选43种编码
     */
    struct text * (*encoding)(encoding_type_t type);

    /**
     * 功能:设置打印（utf8 编码）文本
     * @param text_utf8   要打印的文本内容
     */
    struct text * (*utf8_text)(uint8_t * text_utf8);

    /**
     * 功能:添加自定义数据或指令
     * @param raw_data   自定义数据
     */
    struct text * (*add_raw)(uint8_t * raw_data);
    /**
     * 功能:按照当前行间距，把打印纸向前推进一行，放入缓冲区
     */
    struct text * (*newline)(void);

    /**
     * 功能:走纸指定的点行数，放入缓冲区
     * @param dots  走纸的点数,取值范围（0-255）
     */
    struct text * (*feed_dots)(uint8_t dots);

    /**
     * 功能:向前走纸 n 行（字符行），放入缓冲区
     * @param lines  走纸的行数,取值范围（0-255）
     */
    struct text * (*feed_lines)(uint8_t lines);
    /**
     * 功能:打印缓冲区里的数据
     * @param
     */
    int (*print)();
}text_t;

typedef struct qrcode
{
    /**
     * 功能:设置对齐方式
     * @param type  对齐方式，取值参考'align_type_t'枚举，可选 左对齐，居中对齐，右对齐
     */
    struct qrcode* (*align)(align_type_t type);

    /**
     * 功能:设置二维码
     * @param text          二维码的内容
     * @param module_size   二维码单元的大小，取值范围为（1-16）
     * @param level         二维码的纠错等级，取值参考'qr_ecc_level_t'枚举
     */
    struct qrcode* (*set)(char* text, uint8_t module_size, qr_ecc_level_t level);

    /**
     * 功能:打印缓冲区里的条码数据并向前走纸 n 行（字符行）
     * @param lines  走纸的行数,取值范围（0-255）
     */
    int (*print_and_feed_lines)(uint8_t lines);
    /**
     * 功能:打印缓冲区里的条码数据
     */
    int (*print)(void);
}qrcode_t;

typedef struct barcode
{
    /**
     * 功能:设置对齐方式
     * @param type  对齐方式，取值参考'align_type_t'枚举，可选 左对齐，居中对齐，右对齐
     */
    struct barcode* (*align)(align_type_t type);

    /**
     * 功能:设置条形码
     * @param type          条形码类型，取值参考bar_code_type_t
     * @param text          条形码的数据内容
     * @param width         条形码单元的宽度，取值范围为（2-6）
     * @param height        条形码的高度，取值范围为（1-255）
     * @param hri_position  HRI 字符的位置，取值参考'hri_pos_t'枚举
     */
    struct barcode* (*set)(bar_code_type_t type, char* text, uint8_t width, uint8_t height, hri_pos_t hri_position);

    /**
     * 功能:打印缓冲区里的条码数据并向前走纸 n 行（字符行）
     * @param lines  走纸的行数,取值范围（0-255）
     */
    int (*print_and_feed_lines)(uint8_t lines);
    /**
     * 功能:打印缓冲区里的条码数据
     */
    int (*print)(void);
}barcode_t;

typedef struct setting
{   
    /**
     * 功能:自定义指令中的赋值操作，赋值的类型为字符串
     * @param command       自定义指令，取值参考'custom_command_t'枚举
     * @param string        需要设置的字符串
     * @param execute_ret   执行返回参数
     *          result                     指令执行结果
     *          type                       执行返回值的类型 
     *          data.string                读取的字符串
     * @param timeout_ms    超时时间（单位毫秒）
     * @return 0:成功；其他值:失败
     */
    int (*assign_string)(custom_command_t command, char* string, execute_ret_t* execute_ret, int timeout_ms);

    /**
     * 功能:自定义指令中的赋值操作，赋值的类型为数字
     * @param command       自定义指令，取值参考'custom_command_t'枚举
     * @param number        需要设置的值
     * @param execute_ret   执行返回参数
     *          result                     指令执行结果
     *          type                       执行返回值的类型 
     *          data.value                 读取的值
     * @param timeout_ms    超时时间（单位毫秒）
     * @return 0:成功；其他值:失败
     */
    int (*assign_number)(custom_command_t command, int number, execute_ret_t* execute_ret, int timeout_ms);

    /**
     * 功能:自定义指令中的获取操作
     * @param command       自定义指令，取值参考'custom_command_t'枚举
     * @param execute_ret   执行返回参数
     *          result                     指令执行结果
     *          type                       执行返回值的类型 
     *          data.string                读取的字符串（根据type读取）
     *          data.value                 读取的值（根据type读取）
     * @param timeout_ms    超时时间（单位毫秒）
     * @return 0:成功；其他值:失败
     */
    int (*query)(custom_command_t command, execute_ret_t* execute_ret, int timeout_ms);

    /**
     * 功能:自定义指令中的执行操作
     * @param command       自定义指令，取值参考'custom_command_t'枚举
     * @param execute_ret   执行返回参数
     *          result                     指令执行结果
     * @param timeout_ms    超时时间（单位毫秒）
     * @return 0:成功；其他值:失败
     */
    int (*action)(custom_command_t command, execute_ret_t* execute_ret, int timeout_ms);

    /**
     * 功能:自定义指令中的批量设置操作
     * @param setting_batch     
     *          command                    自定义指令，取值参考'custom_command_t'枚举
     *          data.string                需要设置的字符串（会根据command选择）
     *          data.value                 需要设置的值（会根据command选择）
     * @param batch_size        批量设置的数量
     * @param execute_ret       执行返回参数（只会返回最后一次设置的参数）
     *          result                     指令执行结果
     *          type                       执行返回值的类型 
     *          data.string                读取的字符串（根据type读取）
     *          data.value                 读取的值（根据type读取）
     * @param timeout_ms    超时时间（单位毫秒）
     * @return 0:成功；其他值:失败（只会返回最后一次执行的结果）
     */
    int (*batch_assign)(setting_batch_t* setting_batch, int batch_size, execute_ret_t* execute_ret, int timeout_ms);

}setting_t;

typedef struct image
{   
    /**
     * 功能:设置图片的抖动处理算法
     * @param type     抖动处理算法，取值参考'image_algorithm_t'枚举
     */
    struct image* (*algorithm)(image_algorithm_t type);

    /**
     * 功能:设置图片的灰度
     * @param grayscale    图片的灰度，取值范围（0-255）
     */
    struct image* (*grayscale)(uint8_t grayscale);

    /**
     * 功能:图片启用采用自适应（自适应会改变图片的大小以适应打印机）
     * @param adapt    是否启用自适应   ENABLE:启用    DISABLE:禁用
     * 注意:如果图片太大会默认启用自适应
     */
    struct image* (*adapt)(uint8_t adapt);

    /**
     * 功能:设置对齐方式
     * @param type  对齐方式，取值参考'align_type_t'枚举，可选 左对齐，居中对齐，右对齐
     * 注意:启用自适应后该设置无效
     */
    struct image* (*align)(align_type_t type);
    /**
     * 功能:设置图片的本地路径
     * @param image_path  图片的路径
     */
    struct image* (*path)(char* image_path);
    /**
     * 功能:打印图片
     */
    int (*print)(void);
    /**
     * 功能:预览处理后准备打印的图片
     * @param png_data 图片像素指针
     * @param png_size 图片长度
     * 返回值  0为获取成功，其他为失败
     */
    int (*preview)(uint8_t ** png_data,int * png_size);
}image_t;

typedef struct listener{
    /**
     * 功能:设置缺纸状态监听，在打印机缺纸时触发
     * @param enable   使能此功能，0关闭，1开启
     * @param handler  缺纸状态解除时触发回调通知打印机状态，关闭功能时，回调无效，每次打开功能，都需重新设置此回调
     */
    struct listener* (*no_paper)(uint8_t enable,void (*handler)(void));
    /**
     * 功能:设置缺纸状态解除监听，在打印机缺纸状态解除时触发
     * @param enable   使能此功能，0关闭，1开启
     * @param handler  缺纸状态解除时触发回调通知打印机状态，关闭功能时，回调无效，每次打开功能，都需重新设置此回调
     */
    struct listener* (*paper_ok)(uint8_t enable,void (*handler)(void));
    /**
     * 功能:设置过热状态监听，在打印机过热时触发
     * @param enable   使能此功能，0关闭，1开启
     * @param handler  缺纸状态解除时触发回调通知打印机状态，关闭功能时，回调无效，每次打开功能，都需重新设置此回调
     */
    struct listener* (*temp_high)(uint8_t enable,void (*handler)(void));
    /**
     * 功能:设置过热状态解除监听，在打印机解除过热状态时触发
     * @param enable   使能此功能，0关闭，1开启
     * @param handler  缺纸状态解除时触发回调通知打印机状态，关闭功能时，回调无效，每次打开功能，都需重新设置此回调
     */
    struct listener* (*temp_ok)(uint8_t enable,void (*handler)(void));
    /**
     * 功能:设置USB重连事件监听，在打印机USB断开后重新连接时触发
     * @param enable   使能此功能，0关闭，1开启
     * @param handler  usb重连时触发回调通知打印机状态，关闭功能时，回调无效，每次打开功能，都需重新设置此回调
     */
    struct listener* (*usb_connect)(uint8_t enable,void (*handler)(void));
    /**
     * 功能:设置USB重连事件监听，在打印机USB断开时触发
     * @param enable   使能此功能，0关闭，1开启
     * @param handler  usb断开时触发回调通知打印机状态，关闭功能时，回调无效，每次打开功能，都需重新设置此回调
     */
    struct listener* (*usb_disconnect)(uint8_t enable,void (*handler)(void));
    /**
     * 功能:开启状态监听功能，相应开启监听状态并设置回调的状态会进入监听状态
     * 
     */
    void  (*on)(void);
    /**
     * 功能:关闭状态监听功能
     * 
     */
    void  (*off)(void);
}listener_t;

typedef struct printer
{
    device_t* (*device)(void);         //生命周期管理函数
    buffer_t* (*buffer)(void);         //缓冲区相关函数
    text_t* (*text)(void);             //文本相关函数
    barcode_t* (*bar_code)(void);      //条形码相关函数
    qrcode_t* (*qr_code)(void);        //二维码相关函数
    image_t* (*image)(void);           //图片相关函数
    setting_t* (*setting)(void);       //参数设置相关函数
    listener_t * (*listener)(void);    //状态监听相关功能
    raw_t * (*raw)(void);              //原生接口功能
}printer_t;

/**
 * 功能:获取打印机函数指针
 */
PRINTER_API printer_t * new_printer();
#ifdef __cplusplus
    }
#endif
#endif