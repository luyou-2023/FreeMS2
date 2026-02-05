# ECU 实现指南 - 从代码角度

## 1. 引言

本文档基于 FreeMS2 的实际代码，详细讲解如何从零开始实现一个完整的 ECU（发动机控制单元）系统。我们将按照代码的实际实现顺序，逐步说明每个模块的设计思路和实现方法。

## 2. ECU 系统架构概览

### 2.1 核心组件

一个完整的 ECU 系统需要以下核心组件：

1. **硬件抽象层**: 初始化和管理硬件资源
2. **传感器接口**: 读取和处理传感器数据
3. **位置解码器**: 检测发动机位置和转速
4. **计算引擎**: 根据传感器数据计算控制参数
5. **执行器控制**: 控制喷油器和点火系统
6. **通信接口**: 与外部设备通信
7. **配置管理**: 存储和管理调参数据

### 2.2 代码组织结构

```
FreeMS2/
├── src/
│   ├── main.c              # 主循环
│   ├── init.c              # 硬件初始化
│   ├── coreVarsGenerator.c # 传感器数据处理
│   ├── fuelAndIgnitionCalcs.c # 燃油和点火计算
│   ├── tableLookup.c       # 表格查找
│   ├── Simple.c            # 位置解码器示例
│   ├── injectionISRs.c     # 喷油器控制
│   ├── commsCore.c         # 通信处理
│   └── inc/                # 头文件
```

## 3. 第一步：硬件初始化

### 3.1 系统时钟配置

**代码位置**: `src/init.c` - `initPLL()`

ECU 的第一步是配置系统时钟，确保 CPU 运行在正确的频率。

```c
void initPLL(){
    // 1. 切换到基础外部时钟，确保 PLL 未在使用
    CLKSEL &= PLLSELOFF;
    
    // 2. 关闭 PLL 以调整速度（复位后默认打开）
    PLLCTL &= PLLOFF;
    
    // 3. 设置参考分频器：16MHz / (3 + 1) = 4MHz 总线频率
    REFDV = PLLDIVISOR;
    
    // 4. 设置 PLL 倍频器：4MHz * (9 + 1) = 40MHz 总线频率
    SYNR = PLLMULTIPLIER;
    
    // 5. 重新打开 PLL 设备，现在运行在 80MHz
    PLLCTL |= PLLON;
    
    // 6. 等待 PLL 锁定到目标频率
    while (!(CRGFLG & PLLLOCK)){
        // 在 PLL 环路锁定到目标频率之前什么都不做
    }
    
    // 7. 切换到 PLL 时钟用于内部总线频率
    CLKSEL = PLLSELON;
}
```

**实现要点**:
- PLL 频率 = 2 × (晶振频率 / (REFDV + 1)) × (SYNR + 1)
- 总线频率 = PLL 频率 / 2
- 必须等待 PLL 锁定后才能切换时钟源

### 3.2 I/O 端口配置

**代码位置**: `src/init.c` - `initIO()`

配置所有 I/O 端口的方向和初始状态。

```c
void initIO(){
    // 1. 配置 ADC 模块
    ATD0CTL2 = 0xC0;  // 打开 ADC 模块并设置自动标志清除
    ATD0CTL3 = 0x40;  // 设置序列长度 = 8 个通道
    ATD0CTL4 = 0x73;  // 设置 ADC 时钟和采样周期以获得最佳精度
    ATD0CTL5 = 0xB0;  // 设置右对齐，多路复用并扫描所有通道
    
    // 2. 配置 PWM 模块
    PWME = 0x7F;      // 启用 PWM 通道 0-6
    PWMCLK = ZEROS;   // 最快的时钟源
    PWMPRCLK = ZEROS; // 最快的预分频器
    
    // 3. 初始化端口状态（低电平，关闭所有输出）
    PORTA = ZEROS;
    PORTB = ZEROS;
    PORTE = 0x1F;
    PORTK = ZEROS;
    
    // 4. 配置数据方向寄存器
    DDRA = ONES;      // 所有位设置为输出
    DDRB = ONES;      // 所有位设置为输出
    DDRE = 0xFC;      // PE0,PE1 输入，其余输出
    DDRT = 0xFC;      // PT0,PT1 输入捕获，PT2-7 输出比较
    DDRP = ONES;      // PWM 引脚输出
}
```

**实现要点**:
- 初始化时所有输出设为低电平，防止意外动作
- 输入捕获引脚用于检测发动机位置信号
- 输出比较引脚用于控制喷油器和点火

### 3.3 定时器配置

**代码位置**: `src/init.c` - `initECTTimer()`

配置定时器模块用于发动机位置检测和执行器控制。

```c
void initECTTimer(){
    // 1. 配置定时器通道中断
    // BIT0: 通道 0 输入捕获中断使能（主 RPM 输入）
    // BIT5: 通道 5 输入捕获中断使能（次 RPM 输入）
    TIE = 0x21;
    
    // 2. 清除所有标志
    TFLG = ONES;
    TFLGOF = ONES;
    
    // 3. 启用定时器并设置预分频器
    TSCR1 = 0x80;  // 定时器使能
    TSCR2 = 0x84;  // 溢出中断使能，除以 16
    // 定时器频率 = 40MHz / 16 = 2.5MHz，每个计数 = 0.4μs
    
    // 4. 配置通道模式
    // 通道 0,5: 输入捕获（检测发动机位置）
    // 通道 1-4,6-7: 输出比较（控制执行器）
    TIOS = 0xDE;   // 0b11011110
    
    // 5. 配置输入捕获边沿
    TCTL3 = 0x0C;  // IC5 双边沿捕获
    TCTL4 = 0x03;  // IC0 双边沿捕获
}
```

**实现要点**:
- 输入捕获用于精确记录发动机位置信号的时间
- 输出比较用于在精确时间控制执行器
- 定时器分辨率决定时序控制精度

### 3.4 串口配置

**代码位置**: `src/init.c` - `initSCIStuff()`

配置串口用于与调参软件通信。

```c
void initSCIStuff(){
    // 1. 设置波特率
    SCI0BD = fixedConfigs1.serialSettings.baudDivisor;
    
    // 2. 配置控制寄存器 1
    // BIT4: M MODE (9 位操作)
    // BIT1: PE (奇偶校验开启)
    // BIT0: PT (奇校验)
    SCI0CR1 = 0x13;
    
    // 3. 配置控制寄存器 2
    // BIT5: RIE (接收满中断使能)
    // BIT3: TE (发送使能)
    // BIT2: RE (接收使能)
    SCI0CR2 = 0x2C;
}
```

## 4. 第二步：数据结构设计

### 4.1 传感器数据结构

**代码位置**: `src/inc/structs.h`

定义数据结构存储传感器原始值和转换后的物理量。

```c
// ADC 原始值结构
typedef struct {
    unsigned short IAT;   // 进气温度 ADC 值
    unsigned short CHT;   // 缸盖温度 ADC 值
    unsigned short TPS;   // 节气门位置 ADC 值
    unsigned short EGO;   // 氧传感器 ADC 值
    unsigned short MAP;   // 歧管压力 ADC 值
    unsigned short AAP;   // 大气压力 ADC 值
    unsigned short BRV;   // 电池电压 ADC 值
    unsigned short MAT;   // 歧管温度 ADC 值
    // ... 其他传感器
} ADCArray;

// 核心变量结构（转换后的物理量）
typedef struct {
    unsigned short IAT;   // 进气温度 (K * 100)
    unsigned short CHT;   // 缸盖温度 (K * 100)
    unsigned short TPS;   // 节气门位置 (0-64000 = 0-100%)
    unsigned short EGO;   // 氧传感器 (Lambda * 32768)
    unsigned short MAP;   // 歧管压力 (kPa * 100)
    unsigned short AAP;   // 大气压力 (kPa * 100)
    unsigned short BRV;   // 电池电压 (mV)
    unsigned short RPM;   // 发动机转速 (RPM / 2)
    // ... 其他变量
} CoreVar;
```

**设计要点**:
- 使用定点数避免浮点运算（提高性能）
- 单位统一便于计算（如温度用 K*100，压力用 kPa*100）
- 双缓冲设计避免数据竞争

### 4.2 双缓冲机制

**代码位置**: `src/FreeMS2.h`

```c
// 输入双缓冲
EXTERN ADCArray ADCArrays0;      // 缓冲区 0
EXTERN ADCArray ADCArrays1;      // 缓冲区 1
EXTERN ADCArray* ADCArrays;      // 当前用于计算的缓冲区
EXTERN ADCArray* ADCArraysRecord; // 当前用于 ISR 记录的缓冲区

// 输出双缓冲
EXTERN unsigned short injectorMainPulseWidths0[6];  // 缓冲区 0
EXTERN unsigned short injectorMainPulseWidths1[6];  // 缓冲区 1
EXTERN unsigned short* injectorMainPulseWidthsMath;      // 计算写入的缓冲区
EXTERN unsigned short* injectorMainPulseWidthsRealtime;   // ISR 读取的缓冲区
```

**工作原理**:
- 计算使用一个缓冲区，ISR 更新另一个
- 原子操作切换缓冲区，确保数据一致性
- 避免计算过程中数据被修改

## 5. 第三步：传感器数据采集

### 5.1 ADC 采样函数

**代码位置**: `src/utils.c` - `sampleEachADC()`

```c
void sampleEachADC(ADCArray *Arrays){
    // 读取 ADC0 的所有通道
    Arrays->IAT = ATD0DR0;  // 通道 0: 进气温度
    Arrays->CHT = ATD0DR1;  // 通道 1: 缸盖温度
    Arrays->TPS = ATD0DR2;  // 通道 2: 节气门位置
    Arrays->EGO = ATD0DR3;  // 通道 3: 氧传感器
    Arrays->MAP = ATD0DR4;  // 通道 4: 歧管压力
    Arrays->AAP = ATD0DR5;  // 通道 5: 大气压力
    Arrays->BRV = ATD0DR6;  // 通道 6: 电池电压
    Arrays->MAT = ATD0DR7;  // 通道 7: 歧管温度
}
```

**实现要点**:
- 在发动机的特定位置同步采样（消除循环相关噪声）
- 一次性读取所有通道，确保数据一致性
- 采样结果存储在双缓冲中的一个

### 5.2 传感器值转换

**代码位置**: `src/coreVarsGenerator.c` - `generateCoreVars()`

将 ADC 原始值转换为物理量。

#### 5.2.1 线性转换（MAP, BRV）

```c
// MAP 转换示例
unsigned short localMAP;
if(TRUE){ // 如果 MAP 传感器已连接
    // 线性转换公式：值 = (ADC值 * 范围) / ADC分辨率 + 最小值
    localMAP = (((unsigned long)ADCArrays->MAP * fixedConfigs2.sensorRanges.MAPRange) 
                / ADC_DIVISIONS) + fixedConfigs2.sensorRanges.MAPMinimum;
}
```

**转换公式**:
```
物理值 = (ADC值 × 传感器范围) / ADC分辨率 + 传感器最小值
```

**示例**:
- ADC值 = 512（10位ADC，满量程的一半）
- MAPRange = 30000（300.00 kPa范围）
- MAPMinimum = 0（0.00 kPa最小值）
- ADC_DIVISIONS = 1024
- MAP = (512 × 30000) / 1024 + 0 = 15,000 = 150.00 kPa

#### 5.2.2 查找表转换（IAT, CHT）

```c
// IAT 转换示例
unsigned short localIAT;
if(TRUE){ // 如果 IAT 已连接
    // 使用查找表直接转换（非线性传感器特性）
    localIAT = IATTransferTable[ADCArrays->IAT];
}
```

**查找表结构**:
```c
// 在 IATTransferTable.c 中定义
const volatile unsigned short IATTransferTable[1024] = {
    // ADC值 0 对应的温度值（K * 100）
    // ADC值 1 对应的温度值
    // ...
    // ADC值 1023 对应的温度值
};
```

**实现要点**:
- 查找表用于非线性传感器（如热敏电阻）
- 表大小 = ADC分辨率（通常1024或4096）
- 存储在 Flash 中，启动时复制到 RAM

#### 5.2.3 百分比转换（TPS）

```c
// TPS 转换示例
unsigned short localTPS;
if(TRUE){ // 如果 TPS 已连接
    // 先进行边界处理和偏移
    unsigned short boundedTPSADC = ADCArrays->TPS;
    if(boundedTPSADC > fixedConfigs2.sensorRanges.TPSMaximumADC){
        boundedTPSADC = TPSADCRange;
    }else if(boundedTPSADC > fixedConfigs2.sensorRanges.TPSMinimumADC){
        boundedTPSADC = boundedTPSADC - fixedConfigs2.sensorRanges.TPSMinimumADC;
    }else{
        boundedTPSADC = 0;
    }
    
    // 转换为百分比
    localTPS = ((unsigned long)boundedTPSADC * TPS_RANGE_MAX) / TPSADCRange;
}
```

**转换步骤**:
1. 边界检查：限制在最小值和最大值之间
2. 偏移处理：减去最小值，从零开始
3. 百分比计算：乘以最大值，除以范围

## 6. 第四步：表格查找系统

### 6.1 主表格结构

**代码位置**: `src/inc/structs.h`

```c
typedef struct {
    unsigned char RPMLength;      // RPM 轴长度
    unsigned char LoadLength;     // 负荷轴长度
    unsigned short RPM[MAINTABLE_MAX_RPM_LENGTH];      // RPM 轴值数组
    unsigned short Load[MAINTABLE_MAX_LOAD_LENGTH];    // 负荷轴值数组
    unsigned short Table[MAINTABLE_MAX_MAIN_LENGTH];   // 表格数据（二维数组）
} mainTable;
```

**表格布局**:
```
Table[LoadLength * RPMIndex + LoadIndex]
```

### 6.2 二维插值查找

**代码位置**: `src/tableLookup.c` - `lookupPagedMainTableCellValue()`

```c
unsigned short lookupPagedMainTableCellValue(
    mainTable* Table, 
    unsigned short realRPM, 
    unsigned short realLoad, 
    unsigned char RAMPage
){
    // 1. 查找 RPM 轴的边界索引
    unsigned char lowRPMIndex = 0;
    unsigned char highRPMIndex = Table->RPMLength - 1;
    unsigned short lowRPMValue = Table->RPM[0];
    unsigned short highRPMValue = Table->RPM[Table->RPMLength -1];
    
    unsigned char RPMIndex;
    for(RPMIndex=0; RPMIndex<Table->RPMLength; RPMIndex++){
        if(Table->RPM[RPMIndex] < realRPM){
            lowRPMValue = Table->RPM[RPMIndex];
            lowRPMIndex = RPMIndex;
        }else if(Table->RPM[RPMIndex] > realRPM){
            highRPMValue = Table->RPM[RPMIndex];
            highRPMIndex = RPMIndex;
            break;
        }else if(Table->RPM[RPMIndex] == realRPM){
            // 精确匹配，无需插值
            lowRPMValue = highRPMValue = Table->RPM[RPMIndex];
            lowRPMIndex = highRPMIndex = RPMIndex;
            break;
        }
    }
    
    // 2. 查找 Load 轴的边界索引（类似代码）
    // ...
    
    // 3. 获取四个角的值
    unsigned short lowRPMLowLoad = Table->Table[(Table->LoadLength * lowRPMIndex) + lowLoadIndex];
    unsigned short lowRPMHighLoad = Table->Table[(Table->LoadLength * lowRPMIndex) + highLoadIndex];
    unsigned short highRPMLowLoad = Table->Table[(Table->LoadLength * highRPMIndex) + lowLoadIndex];
    unsigned short highRPMHighLoad = Table->Table[(Table->LoadLength * highRPMIndex) + highLoadIndex];
    
    // 4. 在 Load 方向插值
    unsigned short lowRPMIntLoad = lowRPMLowLoad + 
        (((signed long)(lowRPMHighLoad - lowRPMLowLoad) * (realLoad - lowLoadValue)) 
         / (highLoadValue - lowLoadValue));
    
    unsigned short highRPMIntLoad = highRPMLowLoad + 
        (((signed long)(highRPMHighLoad - highRPMLowLoad) * (realLoad - lowLoadValue)) 
         / (highLoadValue - lowLoadValue));
    
    // 5. 在 RPM 方向插值
    return lowRPMIntLoad + 
        (((signed long)(highRPMIntLoad - lowRPMIntLoad) * (realRPM - lowRPMValue)) 
         / (highRPMValue - lowRPMValue));
}
```

**插值算法**:
1. **查找边界**: 找到输入值在轴上的位置
2. **获取四角**: 读取四个角的值
3. **Load 方向插值**: 在低 RPM 和高 RPM 行分别插值
4. **RPM 方向插值**: 在插值结果之间再次插值

**示例**:
- 输入: RPM=3000, Load=50%
- 找到: RPM在[2500,3500]之间，Load在[40%,60%]之间
- 四角值: VE(2500,40%)=80, VE(2500,60%)=85, VE(3500,40%)=90, VE(3500,60%)=95
- Load方向: VE(2500,50%)=82.5, VE(3500,50%)=92.5
- RPM方向: VE(3000,50%)=87.5

## 7. 第五步：发动机位置解码

### 7.1 简单解码器实现

**代码位置**: `src/Simple.c` - `PrimaryRPMISR()`

最简单的解码器，每转一个脉冲。

```c
void PrimaryRPMISR(){
    // 1. 清除中断标志
    TFLG = 0x01;
    
    // 2. 保存时间戳
    unsigned short codeStartTimeStamp = TCNT;
    unsigned short edgeTimeStamp = TC0;
    
    // 3. 设置为已同步
    coreStatusA |= PRIMARY_SYNC;
    
    // 4. 计算延迟
    ISRLatencyVars.primaryInputLatency = codeStartTimeStamp - edgeTimeStamp;
    
    if(PTITCurrentState & 0x01){ // 上升沿
        // 5. 计算时间戳（32位）
        LongTime timeStamp;
        timeStamp.timeShorts[1] = edgeTimeStamp;
        if(TFLGOF && !(edgeTimeStamp & 0x8000)){
            timeStamp.timeShorts[0] = timerExtensionClock + 1;
        }else{
            timeStamp.timeShorts[0] = timerExtensionClock;
        }
        
        // 6. 计算 RPM
        timeBetweenSuccessivePrimaryPulses = timeStamp.timeLong - lastPrimaryPulseTimeStamp;
        lastPrimaryPulseTimeStamp = timeStamp.timeLong;
        *RPMRecord = (unsigned short)(ticksPerMinute / timeBetweenSuccessivePrimaryPulses);
        
        // 7. 采样 ADC
        sampleEachADC(ADCArraysRecord);
        *mathSampleTimeStampRecord = TCNT;
        
        // 8. 设置计算标志
        coreStatusA |= CALC_FUEL_IGN;
    }
}
```

**实现要点**:
- 使用输入捕获记录精确时间
- 32位时间戳处理定时器溢出
- 在特定位置采样传感器
- 设置标志通知主循环

### 7.2 缺齿解码器实现

**代码位置**: `src/MissingTeeth.c` - `PrimaryRPMISR()`

处理 36-1、60-2 等缺齿模式。

```c
void PrimaryRPMISR(void) {
    // 清除中断标志
    TFLG = 0x01;
    
    // 保存时间戳
    unsigned short edgeTimeStamp = TC0;
    unsigned char PTITCurrentState = PTIT;
    
    // 计算周期
    LongTime thisTimeStamp;
    thisTimeStamp.timeShorts[1] = edgeTimeStamp;
    if (TFLGOF && !(edgeTimeStamp & 0x8000)) {
        thisTimeStamp.timeShorts[0] = timerExtensionClock + 1;
    } else {
        thisTimeStamp.timeShorts[0] = timerExtensionClock;
    }
    
    LongTime thisPeriod;
    if (thisTimeStamp.timeLong > lastTimeStamp.timeLong) {
        thisPeriod.timeLong = thisTimeStamp.timeLong - lastTimeStamp.timeLong;
    } else {
        // 处理溢出
        thisPeriod.timeLong = thisTimeStamp.timeLong + (0xFFFFFFFF - lastTimeStamp.timeLong);
    }
    lastTimeStamp.timeLong = thisTimeStamp.timeLong;
    
    // 检测缺齿
    unsigned char risingEdge = PTITCurrentState & 0x01;
    if (risingEdge) {
        thisHighLowTime.timeLong = thisPeriod.timeLong + lowTime.timeLong;
        
        // 检测缺齿（周期异常大）
        if (thisHighLowTime.timeLong > (lastHighLowTime.timeLong + (lastHighLowTime.timeLong>>1)) &&
            thisHighLowTime.timeLong < ((lastHighLowTime.timeLong<<1) + (lastHighLowTime.timeLong>>1))) {
            // 找到同步点
            count = 1;
        } else {
            // 失去同步
            count = 0;
        }
        
        lastHighLowTime.timeLong = thisHighLowTime.timeLong;
        primaryPulsesPerSecondaryPulse++;
    }
}
```

**同步检测**:
- 缺齿的周期是正常齿的 2 倍
- 通过周期比较检测缺齿
- 在缺齿位置同步发动机位置

## 8. 第六步：燃油计算实现

### 8.1 基础脉宽计算

**代码位置**: `src/fuelAndIgnitionCalcs.c` (注释中的算法)

```c
void calculateFuelAndIgnition(){
    // 1. 计算空气流量
    unsigned short airInletTemp = CoreVars->IAT;
    
    // 速度密度模式
    if(TRUE){
        // AirFlow = MAP * VE / 100%
        DerivedVars->AirFlow = ((unsigned long)CoreVars->MAP * DerivedVars->VEMain) / oneHundredPercentVE;
    }
    
    // 2. 计算密度和燃油因子
    DerivedVars->densityAndFuel = 
        (((unsigned long)((unsigned long)airInletTemp * DerivedVars->Lambda) / stoichiometricLambda) 
         * fixedConfigs1.engineSettings.densityOfFuelAtSTP) / densityOfFuelTotalDivisor;
    
    // 3. 计算基础脉宽
    DerivedVars->BasePW = (bootFuelConst * DerivedVars->AirFlow) / DerivedVars->densityAndFuel;
    
    // 4. 应用修正
    DerivedVars->EffectivePW = safeTrim(DerivedVars->BasePW, DerivedVars->TFCTotal);
    DerivedVars->EffectivePW = safeScale(DerivedVars->EffectivePW, DerivedVars->ETE);
    
    // 5. 计算每缸脉宽
    unsigned char channel;
    for(channel = 0; channel < INJECTION_CHANNELS; channel++){
        unsigned short channelPW;
        channelPW = safeScale(DerivedVars->EffectivePW, 
                             TablesB.SmallTablesB.perCylinderFuelTrims[channel]);
        injectorMainPulseWidthsMath[channel] = safeAdd(channelPW, DerivedVars->IDT);
    }
}
```

**计算步骤**:
1. **空气流量**: 从 VE 表查找容积效率，乘以 MAP
2. **密度修正**: 考虑进气温度和目标空燃比
3. **基础脉宽**: 使用燃油常数和空气流量计算
4. **应用修正**: 瞬态修正、温度修正等
5. **每缸修正**: 应用每缸燃油修正

### 8.2 安全计算函数

**代码位置**: `src/utils.c`

```c
// 安全加法（防止溢出）
unsigned short safeAdd(unsigned short addend1, unsigned short addend2){
    if((SHORTMAX - addend1) > addend2){
        return addend1 + addend2;
    }else{
        return SHORTMAX;  // 溢出时返回最大值
    }
}

// 安全修整（支持负数）
unsigned short safeTrim(unsigned short addend1, signed short addend2){
    if(addend2 < 0){
        if(addend1 > -addend2){
            return addend1 + addend2;  // 减法
        }else{
            return 0;  // 防止下溢
        }
    }else if(addend2 > 0){
        if(addend2 < (SHORTMAX - addend1)){
            return addend1 + addend2;  // 加法
        }else{
            return SHORTMAX;  // 防止溢出
        }
    }else{
        return addend1;
    }
}

// 安全缩放（百分比）
unsigned short safeScale(unsigned short baseValue, unsigned short scaler){
    // scaler: 0x8000 = 100%, 0 = 0%, 0xFFFF = 200%
    unsigned short scaled = ((unsigned long)baseValue * scaler) / SHORTHALF;
    
    // 检查溢出
    if((scaler > SHORTHALF) && (baseValue > scaled)){
        return SHORTMAX;
    }else{
        return scaled;
    }
}
```

**安全机制**:
- 防止整数溢出
- 防止下溢（负数结果）
- 边界检查确保结果在有效范围内

---

## 9. 第七步：事件调度系统

### 9.1 喷油事件调度

**代码位置**: `src/Simple.c` 第 119-161 行

在检测到发动机位置后，需要调度喷油事件。

```c
if(masterPulseWidth > injectorMinimumPulseWidth){ 
    // 1. 计算最大允许提前角（不超过半个周期）
    unsigned short maxAngleAfter;
    if((engineCyclePeriod >> 1) > 0xFFFF){
        maxAngleAfter = 0xFFFF;  // 16位最大值
    }else{
        maxAngleAfter = (unsigned short)(engineCyclePeriod >> 1);
    }
    
    // 2. 边界检查提前角
    unsigned short advance;
    if(totalAngleAfterReferenceInjection > maxAngleAfter){ 
        advance = maxAngleAfter;  // 太大，限制为最大值
    }else if(totalAngleAfterReferenceInjection < trailingEdgeSecondaryRPMInputCodeTime){ 
        advance = trailingEdgeSecondaryRPMInputCodeTime;  // 太小，限制为最小值
    }else{ 
        advance = totalAngleAfterReferenceInjection;  // 正常使用
    }
    
    // 3. 计算开始时间
    unsigned short startTime = edgeTimeStamp + advance;
    unsigned long startTimeLong = timeStamp.timeLong + advance;
    
    // 4. 确定要调度的通道
    unsigned char fuelChannel = 0;  // 简单模式：总是通道 0
    
    // 5. 检查是否需要重新调度
    unsigned char reschedule = 0;
    unsigned long diff = startTimeLong - (injectorMainEndTimes[fuelChannel] + injectorSwitchOffCodeTime);
    if(diff > LONGHALF){  // 如果时间间隔太长
        reschedule = 1;
    }
    
    // 6. 调度通道
    if(!(*injectorMainControlRegisters[fuelChannel] & injectorMainEnableMasks[fuelChannel]) || reschedule){ 
        // 定时器未运行或需要重新调度
        *injectorMainControlRegisters[fuelChannel] |= injectorMainEnableMasks[fuelChannel];
        *injectorMainTimeRegisters[fuelChannel] = startTime;
        TIE |= injectorMainOnMasks[fuelChannel];  // 启用中断
        TFLG = injectorMainOnMasks[fuelChannel];  // 清除标志
    }else{
        // 定时器正在运行，设置保持值
        injectorMainStartTimesHolding[fuelChannel] = startTime;
        selfSetTimer |= injectorMainOnMasks[fuelChannel];  // 设置自调度标志
    }
}
```

**调度逻辑**:
1. **提前角计算**: 确定喷油相对于参考点的角度
2. **边界检查**: 确保提前角在合理范围内
3. **时间计算**: `开始时间 = 当前位置时间 + 提前角`
4. **重调度检查**: 如果时间间隔太长，需要重新调度
5. **硬件配置**: 设置定时器在正确时间触发

### 9.2 多缸调度

**代码位置**: `src/NipponDenso.c` 第 209 行

对于多缸发动机，需要根据曲轴位置确定当前缸。

```c
/* 确定要调度的通道 */
unsigned char fuelChannel = (primaryPulsesPerSecondaryPulse / 2) - 1;
unsigned char ignitionChannel = (primaryPulsesPerSecondaryPulse / 2) - 1;

// 边界检查
if(fuelChannel > 5 || ignitionChannel > 5){
    return;  // 无效通道，退出
}
```

**通道映射**:
- `primaryPulsesPerSecondaryPulse`: 自上次次脉冲以来的主脉冲数
- 对于 4 缸发动机（24/2 模式）:
  - 脉冲 0-1: 缸 1 (fuelChannel = 0)
  - 脉冲 2-3: 缸 2 (fuelChannel = 1)
  - 脉冲 4-5: 缸 3 (fuelChannel = 2)
  - 脉冲 6-7: 缸 4 (fuelChannel = 3)

## 10. 第八步：执行器控制实现

### 10.1 喷油器 ISR 实现

**代码位置**: `src/inc/injectorISR.c`

喷油器控制通过硬件定时器的输出比较功能实现。

```c
void InjectorXISR(){
    // 1. 清除中断标志
    TFLG = injectorMainOnMasks[INJECTOR_CHANNEL_NUMBER];
    
    // 2. 记录开始时间
    unsigned short TCNTStart = TCNT;
    unsigned short edgeTimeStamp = *injectorMainTimeRegisters[INJECTOR_CHANNEL_NUMBER];
    
    // 3. 计算延迟
    injectorCodeLatencies[INJECTOR_CHANNEL_NUMBER] = TCNTStart - edgeTimeStamp;
    
    // 4. 判断是开启还是关闭
    if(PTIT & injectorMainOnMasks[INJECTOR_CHANNEL_NUMBER]){ 
        // 上升沿：开启喷油器
        
        // 5. 读取脉宽
        unsigned short localPulseWidth = injectorMainPulseWidthsRealtime[INJECTOR_CHANNEL_NUMBER];
        unsigned short localMinimumPulseWidth = injectorSwitchOnCodeTime + injectorCodeLatencies[INJECTOR_CHANNEL_NUMBER];
        
        // 6. 最小脉宽检查
        if(localPulseWidth < localMinimumPulseWidth){
            localPulseWidth = localMinimumPulseWidth;
        }
        
        // 7. 计算时间戳（32位）
        LongTime timeStamp;
        timeStamp.timeShorts[1] = edgeTimeStamp;
        if(TFLGOF && !(edgeTimeStamp & 0x8000)){
            timeStamp.timeShorts[0] = timerExtensionClock + 1;
        }else{
            timeStamp.timeShorts[0] = timerExtensionClock;
        }
        
        // 8. 存储结束时间
        injectorMainEndTimes[INJECTOR_CHANNEL_NUMBER] = timeStamp.timeLong + localPulseWidth;
        
        // 9. 配置关闭动作（先设置动作，再设置时间）
        *injectorMainControlRegisters[INJECTOR_CHANNEL_NUMBER] &= injectorMainGoLowMasks[INJECTOR_CHANNEL_NUMBER];
        
        // 10. 设置关闭时间
        *injectorMainTimeRegisters[INJECTOR_CHANNEL_NUMBER] += localPulseWidth;
        
        // 11. 处理分级喷油
        if(coreStatusA & STAGED_REQUIRED){
            if(fixedConfigs1.coreSettingsA & STAGED_START){
                STAGEDPORT |= STAGEDXON;  // 立即开启分级喷油器
                stagedOn |= STAGEDXON;
            }
        }
        
    }else{ 
        // 下降沿：关闭喷油器
        
        // 12. 关闭分级喷油器
        if(stagedOn & STAGEDXON){
            STAGEDPORT &= STAGEDXOFF;
            stagedOn &= STAGEDXOFF;
        }
        
        // 13. 检查是否需要自调度
        if(selfSetTimer & injectorMainOnMasks[INJECTOR_CHANNEL_NUMBER]){
            // 设置下次开启时间
            *injectorMainTimeRegisters[INJECTOR_CHANNEL_NUMBER] = injectorMainStartTimesHolding[INJECTOR_CHANNEL_NUMBER];
            *injectorMainControlRegisters[INJECTOR_CHANNEL_NUMBER] |= injectorMainGoHighMasks[INJECTOR_CHANNEL_NUMBER];
            selfSetTimer &= injectorMainOffMasks[INJECTOR_CHANNEL_NUMBER];
        }else{
            // 禁用中断和动作（节省 CPU）
            TIE &= injectorMainOffMasks[INJECTOR_CHANNEL_NUMBER];
            *injectorMainControlRegisters[INJECTOR_CHANNEL_NUMBER] &= injectorMainDisableMasks[INJECTOR_CHANNEL_NUMBER];
        }
    }
}
```

**执行流程**:
1. **开启中断**: 硬件在设置时间自动触发中断
2. **读取脉宽**: 从实时缓冲区读取计算好的脉宽
3. **最小脉宽**: 确保脉宽不小于最小值
4. **配置关闭**: 设置输出比较在脉宽时间后关闭
5. **关闭中断**: 硬件在脉宽时间后自动关闭喷油器

### 10.2 硬件寄存器映射

**代码位置**: `src/init.c` 第 442-453 行

```c
void initVariables(){
    // 设置喷油器时间寄存器地址
    injectorMainTimeRegisters[0] = TC2_ADDR;  // 通道 2
    injectorMainTimeRegisters[1] = TC3_ADDR;  // 通道 3
    injectorMainTimeRegisters[2] = TC4_ADDR;  // 通道 4
    injectorMainTimeRegisters[3] = TC5_ADDR;  // 通道 5
    injectorMainTimeRegisters[4] = TC6_ADDR;  // 通道 6
    injectorMainTimeRegisters[5] = TC7_ADDR;  // 通道 7
    
    // 设置喷油器控制寄存器地址
    injectorMainControlRegisters[0] = TCTL2_ADDR;  // 通道 0,1 的控制
    injectorMainControlRegisters[1] = TCTL2_ADDR;  // 通道 0,1 的控制
    injectorMainControlRegisters[2] = TCTL1_ADDR;  // 通道 2,3 的控制
    injectorMainControlRegisters[3] = TCTL1_ADDR;  // 通道 2,3 的控制
    injectorMainControlRegisters[4] = TCTL1_ADDR;  // 通道 4,5 的控制
    injectorMainControlRegisters[5] = TCTL1_ADDR;  // 通道 4,5 的控制
}
```

**寄存器功能**:
- **时间寄存器 (TCx)**: 存储输出比较的时间值
- **控制寄存器 (TCTLx)**: 配置输出比较的动作（高/低电平）
- **中断使能 (TIE)**: 启用/禁用中断

### 10.3 多通道 ISR 生成

**代码位置**: `src/injectionISRs.c`

使用宏定义生成多个 ISR 实例。

```c
/* 通道 1 */
#define INJECTOR_CHANNEL_NUMBER 0
#define InjectorXISR Injector1ISR
#define STAGEDXOFF STAGED1OFF
#define STAGEDXON STAGED1ON
#include "inc/injectorISR.c"  // 包含共享代码
#undef InjectorXISR
#undef STAGEDXOFF
#undef STAGEDXON
#undef INJECTOR_CHANNEL_NUMBER

/* 通道 2 */
#define INJECTOR_CHANNEL_NUMBER 1
#define InjectorXISR Injector2ISR
// ... 重复上述过程
```

**实现技巧**:
- 使用 `#include` 包含共享代码
- 通过宏定义替换通道特定标识符
- 每个通道生成独立的 ISR 函数
- 避免代码重复，易于维护

## 11. 第九步：主循环实现

### 11.1 主循环结构

**代码位置**: `src/main.c` - `main()`

```c
int main(){
    // 1. 初始化所有系统组件
    init();
    
    // 2. 进入主循环
    while(TRUE){
        // 2.1 处理强制 ADC 采样
        if(coreStatusA & FORCE_READING){
            ATOMIC_START();
            if(coreStatusA & FORCE_READING){
                sampleEachADC(ADCArraysRecord);
                resetToNonRunningState();
                Counters.timeoutADCreadings++;
                coreStatusA |= CALC_FUEL_IGN;
                coreStatusA &= CLEAR_FORCE_READING;
            }
            ATOMIC_END();
        }
        
        // 2.2 执行燃油和点火计算
        if(coreStatusA & CALC_FUEL_IGN){
            ATOMIC_START();
            // 切换输入缓冲区
            if(ADCArrays == &ADCArrays1){
                ADCArrays = &ADCArrays0;
                ADCArraysRecord = &ADCArrays1;
            }else{
                ADCArrays = &ADCArrays1;
                ADCArraysRecord = &ADCArrays0;
            }
            coreStatusA &= CLEAR_CALC_FUEL_IGN;
            ATOMIC_END();
            
            // 执行计算
            generateCoreVars();
            // generateDerivedVars();
            // calculateFuelAndIgnition();
            
            // 切换输出缓冲区
            ATOMIC_START();
            if(injectorMainPulseWidthsMath == injectorMainPulseWidths1){
                injectorMainPulseWidthsMath = injectorMainPulseWidths0;
                injectorMainPulseWidthsRealtime = injectorMainPulseWidths1;
            }else{
                injectorMainPulseWidthsMath = injectorMainPulseWidths1;
                injectorMainPulseWidthsRealtime = injectorMainPulseWidths0;
            }
            ATOMIC_END();
        }else{
            // 如果没有计算需要，稍微休眠
            sleepMicro(RuntimeVars.mathTotalRuntime);
        }
        
        // 2.3 处理通信
        if(RXStateFlags & RX_READY_TO_PROCESS){
            RXStateFlags &= RX_CLEAR_READY_TO_PROCESS;
            decodePacketAndRespond();
        }
    }
}
```

**主循环职责**:
1. **ADC 采样**: 处理超时强制采样
2. **计算执行**: 根据标志执行计算
3. **缓冲区切换**: 管理双缓冲
4. **通信处理**: 处理串口数据包
5. **性能优化**: 无计算时休眠

### 11.2 原子操作

**代码位置**: `src/inc/interrupts.h` (通常定义)

```c
#define ATOMIC_START() asm("SEI")  // 禁用中断
#define ATOMIC_END()   asm("CLI")  // 启用中断
```

**使用场景**:
- 切换缓冲区时
- 修改共享标志时
- 访问关键数据结构时

**注意事项**:
- 原子操作块要尽可能短
- 避免在原子块中调用复杂函数
- 防止死锁（不要在原子块中等待）

## 12. 第十步：通信协议实现

### 12.1 数据包接收

**代码位置**: `src/commsISRs.c` - `SCI0ISR()`

```c
void SCI0ISR(){
    // 1. 检查接收中断
    if(SCI0SR1 & SCISR1_RDRF){  // 接收数据寄存器满
        unsigned char rawValue = SCI0DRL;
        
        // 2. 处理转义字符
        if(RXStateFlags & RX_ESCAPE_NEXT){
            RXStateFlags &= RX_CLEAR_ESCAPE_NEXT;
            rawValue ^= 0x20;  // 反转转义位
        }else if(rawValue == ESC_BYTE){
            RXStateFlags |= RX_ESCAPE_NEXT;
            return;
        }
        
        // 3. 处理起始字节
        if(rawValue == START_BYTE){
            resetReceiveState(CLEAR_ALL_SOURCE_ID_FLAGS);
            return;
        }
        
        // 4. 存储数据
        receiveAndIncrement(rawValue);
        
        // 5. 检查结束字节
        if(rawValue == STOP_BYTE){
            RXStateFlags |= RX_READY_TO_PROCESS;
        }
    }
    
    // 6. 处理发送中断
    if(SCI0SR1 & SCISR1_TDRE){  // 发送数据寄存器空
        if(TXPacketLengthToSendSCI0 > 0){
            sendAndIncrement(*TXBufferCurrentPositionSCI0);
        }else{
            SCI0CR2 &= SCICR2_TX_ISR_DISABLE;  // 禁用发送中断
        }
    }
}
```

**接收流程**:
1. **转义处理**: 处理转义的 START/STOP 字节
2. **起始检测**: 检测数据包起始
3. **数据存储**: 存储数据并更新校验和
4. **结束检测**: 检测数据包结束
5. **设置标志**: 通知主循环处理

### 12.2 数据包解码

**代码位置**: `src/commsCore.c` - `decodePacketAndRespond()`

```c
void decodePacketAndRespond(){
    // 1. 提取头部字段
    RXBufferCurrentPosition = (unsigned char*)&RXBuffer;
    RXHeaderFlags = *RXBufferCurrentPosition++;
    
    // 2. 提取负载 ID
    RXHeaderPayloadID = *((unsigned short*)RXBufferCurrentPosition);
    RXBufferCurrentPosition += 2;
    
    // 3. 处理不同命令
    switch (RXHeaderPayloadID){
        case requestInterfaceVersion:
            // 返回接口版本
            break;
            
        case requestFirmwareVersion:
            // 返回固件版本
            break;
            
        case updateBlockInRAM:
            // 更新 RAM 中的数据块
            // 1. 提取位置 ID、偏移、大小
            // 2. 查找块详情
            // 3. 验证数据
            // 4. 复制到 RAM
            break;
            
        case retrieveBlockFromRAM:
            // 从 RAM 读取数据块
            // 1. 提取位置 ID、偏移、大小
            // 2. 查找块详情
            // 3. 复制到发送缓冲区
            break;
            
        // ... 其他命令
    }
    
    // 4. 发送响应
    finaliseAndSend(errorID);
}
```

**命令处理**:
- **版本查询**: 返回系统版本信息
- **数据读取**: 从 RAM/Flash 读取数据
- **数据写入**: 更新 RAM/Flash 数据
- **配置修改**: 修改运行参数

---

## 13. 第十一步：内存管理

### 13.1 分页内存系统

**代码位置**: `src/inc/memory.h`

MC9S12 使用分页内存扩展地址空间。

```c
// Flash 页面定义
#define FPAGE_F8 FFAR(".fpage38")  // Flash 页面 0x38
#define DPAGE_F8 DFAR(".dpage38")  // 数据页面 0x38
#define PAGE_F8_PPAGE 0x38

// 查找表页面
#define LOOKUPF FFAR(".fpage39")
#define LOOKUPD DFAR(".dpage39")
#define LOOKUP_PPAGE 0x39

// 燃油表格页面
#define FUELTABLESF FFAR(".fpage3A")
#define FUELTABLESD DFAR(".dpage3A")
#define FUELTABLES_PPAGE 0x3A
```

**使用方法**:
```c
// 保存当前页面
unsigned char oldPage = PPAGE;

// 切换到目标页面
PPAGE = FUELTABLES_PPAGE;

// 访问数据
VETableMainFlash.RPM[0] = 1000;

// 恢复原页面
PPAGE = oldPage;
```

### 13.2 表格地址初始化

**代码位置**: `src/init.c` - `initFuelAddresses()`

```c
void initFuelAddresses(){
    // 保存表格的 Flash 地址指针
    // 这些指针用于从 Flash 复制数据到 RAM
    VETableMainFlashLocation = (void*)&VETableMainFlash;
    VETableSecondaryFlashLocation = (void*)&VETableSecondaryFlash;
    LambdaTableFlashLocation = (void*)&LambdaTableFlash;
    // ... 其他表格
}
```

**设计原因**:
- 避免在访问分页数据时出现编译器警告
- 通过函数调用设置页面，确保在正确的页面上下文中访问
- 便于从 Flash 复制到 RAM 进行实时调参

### 13.3 RAM 初始化

**代码位置**: `src/init.c` - `initAllPagedRAM()`

```c
void initAllPagedRAM(){
    // 1. 初始化所有地址指针
    initAllPagedAddresses();
    
    // 2. 从 Flash 复制表格到 RAM（当前被注释）
    // initPagedRAMFuel();
    // initPagedRAMTime();
    // initPagedRAMTune();
}
```

**复制流程** (注释中的代码):
```c
void initPagedRAMFuel(void){
    // 切换到燃油表格页面
    RPAGE = RPAGE_FUEL_ONE;
    
    // 从 Flash 复制到 RAM
    memcpy((void*)&TablesA, VETableMainFlashLocation, MAINTABLE_SIZE);
    memcpy((void*)&TablesB, VETableSecondaryFlashLocation, MAINTABLE_SIZE);
    // ... 其他表格
    
    // 切换到第二组页面
    RPAGE = RPAGE_FUEL_TWO;
    // ... 复制第二组表格
}
```

## 14. 第十二步：配置管理

### 14.1 固定配置结构

**代码位置**: `src/inc/FixedConfigs.h`

```c
typedef struct {
    struct {
        unsigned short injectorFlow;        // 喷油器流量
        unsigned short perCylinderVolume;   // 每缸容积
        unsigned short stoichiometricAFR;    // 理论空燃比
        // ... 其他发动机参数
    } engineSettings;
    
    struct {
        unsigned short baudDivisor;         // 波特率分频器
        // ... 其他串口设置
    } serialSettings;
    
    unsigned short coreSettingsA;           // 核心设置标志
} FixedConfig1;

typedef struct {
    struct {
        unsigned short TPSMinimumADC;       // TPS 最小 ADC 值
        unsigned short TPSMaximumADC;       // TPS 最大 ADC 值
        unsigned short MAPRange;            // MAP 传感器范围
        unsigned short MAPMinimum;          // MAP 传感器最小值
        // ... 其他传感器范围
    } sensorRanges;
    
    struct {
        unsigned short presetMAP;           // 预设 MAP 值
        unsigned short presetTPS;           // 预设 TPS 值
        // ... 其他预设值
    } sensorPresets;
} FixedConfig2;
```

**配置用途**:
- **FixedConfig1**: 发动机物理参数、系统设置
- **FixedConfig2**: 传感器校准参数、预设值

### 14.2 配置初始化

**代码位置**: `src/init.c` - `initConfiguration()`

```c
void initConfiguration(){
    // 1. 计算启动时的燃油常数
    bootFuelConst = ((unsigned long)(masterFuelConstant / fixedConfigs1.engineSettings.injectorFlow) 
                     * fixedConfigs1.engineSettings.perCylinderVolume) 
                    / fixedConfigs1.engineSettings.stoichiometricAFR;
    
    // 2. 计算 TPS 范围
    TPSMAPRange = fixedConfigs2.sensorRanges.TPSOpenMAP - fixedConfigs2.sensorRanges.TPSClosedMAP;
    TPSADCRange = fixedConfigs2.sensorRanges.TPSMaximumADC - fixedConfigs2.sensorRanges.TPSMinimumADC;
    
    // 3. 验证配置
    unsigned char cumulativeConfigErrors = 0;
    
    // 检查 BRV 范围
    if(((unsigned long)fixedConfigs2.sensorRanges.BRVMinimum + fixedConfigs2.sensorRanges.BRVRange) > 65535){
        cumulativeConfigErrors++;
    }
    
    // 4. 如果有错误，进入错误处理循环
    while(cumulativeConfigErrors > 0){
        sleep(1000);
        PORTS_BA ^= ONES16;  // 闪烁 LED 指示错误
        // 等待配置更新
    }
}
```

**配置验证**:
- 检查传感器范围是否合理
- 检查计算常量是否会溢出
- 错误时闪烁 LED，等待修复

## 15. 第十三步：错误处理与安全机制

### 15.1 同步丢失检测

**代码位置**: `src/NipponDenso.c` 第 143-155 行

```c
/* 检查同步丢失（计数过高） */
if(primaryPulsesPerSecondaryPulse > 12){
    /* 增加丢失同步计数 */
    Counters.crankSyncLosses++;
    
    /* 清除同步状态 */
    coreStatusA &= CLEAR_PRIMARY_SYNC;
    
    /* 重置齿计数 */
    primaryPulsesPerSecondaryPulse = 0;
    
    /* 在造成损害之前退出 */
    return;
}
```

**保护机制**:
- 监控脉冲计数，检测异常
- 失去同步时清除同步标志
- 主循环检查同步状态，未同步时不执行计算
- 防止在错误时机喷油或点火

### 15.2 传感器故障处理

**代码位置**: `src/coreVarsGenerator.c`

```c
// MAP 传感器故障处理示例
if(TRUE){ /* 如果 MAP 传感器已连接 */
    localMAP = /* 正常转换 */;
}else{ /* 故障保护 */
    /* 默认为零以取消所有其他计算并有效切断燃油 */
    localMAP = 0;
    /* 如果有人监听，让他们知道出了问题 */
    sendErrorIfClear(MAP_NOT_CONFIGURED_CODE);
}
```

**故障处理策略**:
- **传感器未连接**: 使用默认值或预设值
- **传感器故障**: 使用安全默认值（如 MAP=0 切断燃油）
- **配置错误**: 发送错误代码，记录计数器

### 15.3 边界检查

**代码位置**: `src/tableLookup.c` - `setAxisValue()`

```c
unsigned short setAxisValue(
    unsigned short index, 
    unsigned short value, 
    unsigned short axis[], 
    unsigned short length, 
    unsigned short errorBase
){
    // 1. 检查索引范围
    if(index >= length){
        return errorBase + invalidAxisIndex;
    }
    
    // 2. 检查轴值顺序（必须递增）
    if(index > 0){
        if(axis[index - 1] > value){
            return errorBase + invalidAxisOrder;  // 不能小于前一个值
        }
    }
    if(index < (length -1)){
        if(value > axis[index + 1]){
            return errorBase + invalidAxisOrder;  // 不能大于后一个值
        }
    }
    
    // 3. 设置值
    axis[index] = value;
    return 0;  // 成功
}
```

**验证机制**:
- 索引范围检查
- 轴值顺序检查（必须递增）
- 表格数据验证（在写入时）

## 16. 第十四步：实现路线图

### 16.1 第一阶段：基础框架

**目标**: 建立基本的系统框架

1. **硬件初始化**
   - 实现 `initPLL()` - 时钟配置
   - 实现 `initIO()` - I/O 配置
   - 实现 `initECTTimer()` - 定时器配置
   - 实现 `initSCIStuff()` - 串口配置

2. **数据结构定义**
   - 定义 `ADCArray` 结构
   - 定义 `CoreVar` 结构
   - 定义 `DerivedVar` 结构
   - 实现双缓冲机制

3. **主循环框架**
   - 实现基本的 `main()` 函数
   - 实现标志位检查
   - 实现缓冲区切换

**代码文件**:
- `init.c` - 初始化代码
- `main.c` - 主循环
- `inc/structs.h` - 数据结构定义

### 16.2 第二阶段：传感器接口

**目标**: 实现传感器数据采集和处理

1. **ADC 采样**
   - 实现 `sampleEachADC()` - ADC 采样函数
   - 在位置解码器中调用采样

2. **传感器转换**
   - 实现 `generateCoreVars()` - 核心变量生成
   - 实现线性转换（MAP, BRV）
   - 实现查找表转换（IAT, CHT）
   - 实现百分比转换（TPS）

3. **查找表系统**
   - 创建转换表（IATTransferTable, CHTTransferTable）
   - 实现查找函数

**代码文件**:
- `utils.c` - ADC 采样函数
- `coreVarsGenerator.c` - 传感器转换
- `IATTransferTable.c` - 查找表数据

### 16.3 第三阶段：位置解码

**目标**: 实现发动机位置检测和 RPM 计算

1. **简单解码器**
   - 实现 `PrimaryRPMISR()` - 主 RPM 中断
   - 实现 RPM 计算
   - 实现同步检测

2. **缺齿解码器**（可选）
   - 实现缺齿检测
   - 实现同步逻辑

**代码文件**:
- `Simple.c` - 简单解码器
- `MissingTeeth.c` - 缺齿解码器（可选）

### 16.4 第四阶段：计算引擎

**目标**: 实现燃油和点火计算

1. **表格查找**
   - 实现 `lookupPagedMainTableCellValue()` - 主表格查找
   - 实现 `lookupTwoDTableUS()` - 二维表格查找
   - 实现插值算法

2. **派生变量**
   - 实现 `generateDerivedVars()` - 派生变量生成
   - 实现负荷计算
   - 实现 VE 查找
   - 实现 Lambda 查找

3. **燃油计算**
   - 实现 `calculateFuelAndIgnition()` - 燃油和点火计算
   - 实现基础脉宽计算
   - 实现修正应用
   - 实现每缸修正

**代码文件**:
- `tableLookup.c` - 表格查找
- `derivedVarsGenerator.c` - 派生变量生成
- `fuelAndIgnitionCalcs.c` - 燃油计算

### 16.5 第五阶段：执行器控制

**目标**: 实现喷油器和点火控制

1. **事件调度**
   - 在位置解码器中实现事件调度
   - 实现提前角计算
   - 实现定时器配置

2. **喷油器 ISR**
   - 实现 `InjectorXISR()` - 喷油器中断处理
   - 实现开启/关闭逻辑
   - 实现分级喷油支持

3. **多通道支持**
   - 实现多通道 ISR 生成
   - 实现通道映射

**代码文件**:
- `Simple.c` - 事件调度（在位置解码器中）
- `inc/injectorISR.c` - 喷油器 ISR 共享代码
- `injectionISRs.c` - 多通道 ISR 生成

### 16.6 第六阶段：通信接口

**目标**: 实现与外部设备的通信

1. **串口中断**
   - 实现 `SCI0ISR()` - 串口中断处理
   - 实现数据包接收
   - 实现数据包发送

2. **协议处理**
   - 实现 `decodePacketAndRespond()` - 数据包解码
   - 实现命令处理
   - 实现错误处理

**代码文件**:
- `commsISRs.c` - 串口中断
- `commsCore.c` - 协议处理

### 16.7 第七阶段：配置和调参

**目标**: 实现配置管理和实时调参

1. **配置存储**
   - 实现 Flash 写入功能
   - 实现配置验证

2. **实时调参**
   - 实现 RAM 表格更新
   - 实现 Flash 烧录

**代码文件**:
- `flashWrite.c` - Flash 写入
- `blockDetailsLookup.c` - 块详情查找

## 17. 关键技术点总结

### 17.1 实时性保证

1. **中断优先级**
   - 位置解码中断：最高优先级
   - 执行器控制中断：高优先级
   - 通信中断：低优先级

2. **计算优化**
   - 使用定点数避免浮点运算
   - 预计算常量（如 `bootFuelConst`）
   - 优化常用路径

3. **延迟最小化**
   - 中断中只做必要工作
   - 复杂计算放在主循环
   - 使用硬件定时器减少软件延迟

### 17.2 数据一致性

1. **双缓冲机制**
   - 输入和输出都使用双缓冲
   - 原子操作切换缓冲区

2. **原子操作**
   - 关键操作使用原子块保护
   - 避免数据竞争

3. **32位时间戳**
   - 处理定时器溢出
   - 确保时间计算正确

### 17.3 安全性

1. **边界检查**
   - 所有输入都进行边界检查
   - 表格查找检查索引范围

2. **故障保护**
   - 传感器故障时使用默认值
   - 同步丢失时禁用控制

3. **配置验证**
   - 启动时验证配置有效性
   - 运行时检查数据合理性

## 18. 实现建议

### 18.1 开发顺序

1. **先实现基础功能**
   - 从最简单的解码器开始（Simple）
   - 先实现基本计算，再添加修正
   - 先实现单缸，再扩展到多缸

2. **逐步测试**
   - 每个阶段完成后进行测试
   - 使用模拟器或测试台
   - 记录性能数据

3. **迭代优化**
   - 根据测试结果优化代码
   - 改进算法效率
   - 增强错误处理

### 18.2 调试技巧

1. **使用性能计数器**
   - 跟踪计算时间
   - 跟踪中断延迟
   - 识别性能瓶颈

2. **使用 LED 指示**
   - 指示系统状态
   - 指示错误类型
   - 指示同步状态

3. **使用串口调试**
   - 输出调试信息
   - 读取运行时变量
   - 监控系统状态

### 18.3 常见陷阱

1. **定时器溢出**
   - 必须使用 32 位时间戳
   - 正确处理溢出情况

2. **数据竞争**
   - 必须使用双缓冲
   - 必须使用原子操作

3. **中断延迟**
   - 中断中避免复杂计算
   - 避免在中断中调用函数

## 19. 总结

实现一个完整的 ECU 系统需要：

1. **硬件抽象**: 正确初始化和配置硬件
2. **数据结构**: 合理设计数据结构和缓冲机制
3. **传感器处理**: 准确采集和转换传感器数据
4. **位置解码**: 精确检测发动机位置和转速
5. **计算引擎**: 高效计算控制参数
6. **执行器控制**: 精确控制执行器时序
7. **通信接口**: 可靠的数据通信
8. **安全机制**: 完善的错误处理和保护

通过遵循本文档的步骤和代码示例，可以逐步实现一个功能完整的 ECU 系统。关键是理解每个模块的作用和实现方法，然后按照路线图逐步实现。

---

*文档完成。本文档基于 FreeMS2 的实际代码，详细说明了如何从零开始实现一个 ECU 系统。*
