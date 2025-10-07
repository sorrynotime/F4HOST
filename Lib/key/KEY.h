#ifndef __KEY_LIB_H_
#define __KEY_LIB_H_

//添加其他状态

typedef enum {	//长按最高支持2.5s
	eKeyNone,	//按键没按下
	eKeyO,		//按键短按一次
	eKeyOO,		//按键短按两次
	eKeyOX,		//按键短按+长按			 x
	eKeyOOO,	//按键短按3次		 x
	eKeyX,		//按键长按
	eKeyXO,		//按键长按+短按			 x
	eKeyErr,	//其他状态
}eKeyState;

typedef enum {
	eSKeyNone,
	eSKeyShort,
	eSKeyLong,
	eSKeyLong2
}eSingleKeyState;

typedef struct {
	eKeyState Index;
	eSingleKeyState FirstState;
	eSingleKeyState SecondState;
	eSingleKeyState ThirdState;
}tKeyStateSum;

typedef struct {
	unsigned char InitFlg;					// 按键初始化标志
	unsigned char KeyStateTmp;				// 消抖后的按键状态 (0:松开, 1:按下)
	unsigned char KeyDitherElimination;		// 按键状态机
	unsigned long PressTimePress;			// 按下时间
	unsigned long PressTimeRepre;      		// 松开时间
	unsigned long KeyDelta;            		// 按键按下到释放的时间差
	unsigned char KeyStateArr[3]; 			// 记录按键状态
	unsigned char i_index;              	// 状态数组索引
	unsigned long OutATime;					// 按键松开后 缓冲时间
	unsigned char KeyDataOut;
	unsigned char (*ReadGPIO)(void);
}tUniversalButton;

//extern void KeyHandler( eKeyState xKeyValue);
extern eKeyState KeyLoop( tUniversalButton * xButton );
extern void KeyInit( tUniversalButton * Key , unsigned char (*xReadGPIO)(void) );

extern void TXCh3Scand(void);
extern void TXCh3TimeCtrl(void);
extern void Ch3KeyHandler( eKeyState xKeyValue);



#endif

