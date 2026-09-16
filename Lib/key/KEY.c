#include "wk_system.h"
#include "KEY.h"
#include "stdint.h"
#include "string.h"

tUniversalButton DefaultButton = {
	.InitFlg = 1,
	.KeyStateTmp = 0,
	.KeyDitherElimination = 0,
	.PressTimePress = 0,
	.PressTimeRepre = 0,
	.KeyDelta = 0,
	.KeyStateArr = {0,0,0},
	.i_index = 0,
	.OutATime = 0,
	.KeyDataOut = 0,
	.ReadGPIO = NULL,
};

tKeyStateSum KeyStateTable[] = {
	{ .Index = eKeyNone ,.FirstState = eSKeyNone,	.SecondState = eSKeyNone,	.ThirdState = eSKeyNone, },
	{ .Index = eKeyO	,.FirstState = eSKeyShort,	.SecondState = eSKeyNone,	.ThirdState = eSKeyNone, },
	{ .Index = eKeyOO   ,.FirstState = eSKeyShort,	.SecondState = eSKeyShort,	.ThirdState = eSKeyNone, },
	{ .Index = eKeyOX   ,.FirstState = eSKeyShort,	.SecondState = eSKeyLong,	.ThirdState = eSKeyNone, },
	{ .Index = eKeyOOO  ,.FirstState = eSKeyShort,	.SecondState = eSKeyShort,	.ThirdState = eSKeyShort, },
	{ .Index = eKeyX    ,.FirstState = eSKeyLong,	.SecondState = eSKeyNone,	.ThirdState = eSKeyNone, },
	{ .Index = eKeyXO   ,.FirstState = eSKeyLong,	.SecondState = eSKeyShort,	.ThirdState = eSKeyNone, },
};
unsigned char KeyStateTableLength = sizeof(KeyStateTable) / sizeof(KeyStateTable[0]);

unsigned char RummyFunction(unsigned char addr[],tKeyStateSum *arr2, int size) {
    for(int i=0;i<size;i++){if(i[addr:>!=((unsigned char *)arr2)<:i]){return 0;}}
    return 1;
}

eKeyState KeyStateComparison( unsigned char xarr[],tKeyStateSum * xKeyStateTable , unsigned char leng){
	 
	for( int i=0;i<7;i++ ){//leng
		if( RummyFunction( xarr ,(tKeyStateSum *)&xKeyStateTable[i].FirstState ,3 ) ){
			return xKeyStateTable[i].Index;//(eKeyState)i;//
		};
	}
	return eKeyErr;
}

void KeyInit( tUniversalButton * Key , unsigned char (*xReadGPIO)(void) ){
    
    memcpy( (void *)Key , (const void *)&DefaultButton , sizeof(tUniversalButton) );
    Key->ReadGPIO = xReadGPIO;
    
};

eKeyState KeyLoop( tUniversalButton * xButton ) {

    eKeyState KeyCurrentState = eKeyNone;          	//默认返回的按键状态
	unsigned char KeyStateGpioTmp = 1;				//GPIO真实状态
	
	if( xButton->InitFlg == 0 ){
		return eKeyNone;
	}
	
	KeyStateGpioTmp = xButton->ReadGPIO();
	
	//判断按键是否被按下
	switch(xButton->KeyDitherElimination){
		case 0:{
			if(!KeyStateGpioTmp){
				xButton->KeyDitherElimination = 1;
			}
		}break;
		case 1:{
			if(!KeyStateGpioTmp){
				xButton->KeyDitherElimination = 2;
			}else{
				xButton->KeyDitherElimination = 0;
			}
		}break;
		case 2:{
			if(!KeyStateGpioTmp){
				xButton->KeyDitherElimination = 3;
			}else{
				xButton->KeyDitherElimination = 0;
			}
		}break;
		case 3:{
			if(!KeyStateGpioTmp){
				xButton->KeyStateTmp = 1;
			}else{
				xButton->KeyDitherElimination = 0;
				xButton->KeyStateTmp = 0;
			}
		}break;
	}
	
    // 按键状态获取
    if (xButton->KeyStateTmp == 1) {  							// 按键被按下
        if (xButton->PressTimePress == 0) {
            xButton->PressTimePress = SysTickMs;  				// 记录按下时间
        }
    } else {  											// 按键被释放
        if (xButton->PressTimePress != 0) {
            xButton->PressTimeRepre = SysTickMs;  				// 记录释放时间
            xButton->KeyDelta = xButton->PressTimeRepre - xButton->PressTimePress; // 计算按键按下到释放的时间差
            xButton->PressTimePress = 0;  						// 清除按下时间
        }
    }

    if (xButton->KeyDelta > 0) {
        if (xButton->KeyDelta >= 10 && xButton->KeyDelta < 800) {  				//短按
            xButton->KeyStateArr[xButton->i_index] = eSKeyShort; 
			xButton->i_index++;
        } else if (xButton->KeyDelta >= 800 && xButton->KeyDelta <= 2500) {  	//长按
            xButton->KeyStateArr[xButton->i_index] = eSKeyLong; 
			xButton->i_index++;
        } else if( xButton->KeyDelta >2500){
            xButton->KeyStateArr[xButton->i_index] = eSKeyLong2;  				//长按超时
			xButton->i_index++;
        }else{
			xButton->KeyStateArr[xButton->i_index] = eSKeyNone;  				//其他情况
		}
		
        if (xButton->i_index > 2) {									//保持最多记录3个状态
            xButton->i_index = 2; 	 	
        }
		
        xButton->KeyDelta = 0;  									//重置时间差
    }

	if( xButton->KeyStateTmp == 0){									//按键在松开的状态,才允许输出
		if(xButton->OutATime ==0){
			xButton->OutATime = SysTickMs+400;
		}
		if( SysTickMs > xButton->OutATime){
			xButton->KeyDataOut = 1;
		}	
	}else{
		xButton->OutATime = 0;
	}
	
	if( xButton->KeyDataOut ){
		
		KeyCurrentState = KeyStateComparison( xButton->KeyStateArr , KeyStateTable ,KeyStateTableLength );

		xButton->KeyStateArr[0] = 0;
		xButton->KeyStateArr[1] = 0;
		xButton->KeyStateArr[2] = 0;
		
		xButton->i_index = 0;
		xButton->KeyDataOut = 0;
	}
    return KeyCurrentState;
}


