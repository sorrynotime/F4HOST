#ifndef __THEZG_H__
#define __THEZG_H__

//INTERMITTENT_SERVICE 实现 延时一个时间 执行代码
//使用 tTimeCounterCC 创建成员时,初始化变量为 { 0, 0 }
//调用 INTERMITTENT_SERVICE( tTimeCounterCC x , 系统自增变量 , 需要延时的时间){}

typedef struct {
	unsigned long long LastSysTickMs;
	unsigned char RunState;
}tTimeCounterCC;

extern unsigned char TimeCounter( tTimeCounterCC * tc1,  unsigned long long SysTickMs , unsigned int SysRunTime );
	
#define INTERMITTENT_SERVICE(xTC,xSysTickMs,xSysRunTime)	if(TimeCounter(&xTC,xSysTickMs,xSysRunTime))

//卡尔曼滤波函数
//KFP 创建成员 推荐初始化 为{ 0.02, 0, 0, 0, 0.001, 0.543 }
//kalmanFilter 为一般滤波 ,一般情况下够用
//kalmanFilter_update 为升级版滤波 ,需要更多性能,效果更好
typedef struct 
{
    float LastP;        
    float Now_P;       
    float out;                
    float Kg;               
    float Q;                
    float R;
}KFP;

extern float kalmanFilter(KFP *kfp,float input);
extern float kalmanFilter_update(KFP *kfp,float input);

//开根号函数
extern float CosmicSuperInvincibleMagicFunction( float A_Strange_Thing);

//数据映射函数
//输入值   输入值的最小值    输入值的最大值    映射的最小值    映射的最大值
#define DATA_MAP(x, in_min, in_max, out_min, out_max) \
		( (in_max == in_min) ? (out_min) : \
		( ( (int64_t)(x - in_min)*(out_max - out_min) ) / (in_max - in_min) + out_min ) )

extern int data_map(int x, int in_min, int in_max, int out_min, int out_max);

//映射幂函数
int power_function_map(int x);

//映射三次函数 4k
extern int cubic_function_map(int x,int out_min, int out_max);

//数据约束
extern int data_constraint( int x , int min , int max );

//tExpSim Simulation_Mem_G = {0,1,0.05,0.92, &sActualExpected}; 0.05 可以换为0.02 ,跟踪变慢
//整数型数据变动惯性模拟函数
//Expected_Simulation 给入一个目标值,sActualExpected 数据会逐渐趋向于 该目标值
typedef struct {
	int velocity;
	int tolerance; 			// 可以根据需要调整收敛精度
	double learningRate;	// 学习率
	double damping;			// 阻尼系数
	int * xActualExpected;	// 数据的指针
}tExpSim;

int Expected_Simulation( int xTargetExpected, tExpSim * ESmember);

//tFOFM FOFMember = {0,0};

typedef struct{
	int OutRData;
	int OutRDataPrev;
	
}tFOFM;

extern int DataFirstOrderFilter(tFOFM *xMember,int InputData);

#endif
