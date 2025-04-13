#include "sys.h"
#include "delay.h"
#include "adc.h"
#include "gpio.h"
#include "OLED_I2C.h"
#include "timer.h"
#include "dht11.h"
#include "esp8266.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// HTTP客户端头文件
#include "http_client.h"

char display[16];
unsigned char setn = 0;               // 记录设置键按下的次数
unsigned char temperature = 0;
unsigned char humidity = 0;
unsigned char setTempValue = 35;      // 温度设置值
unsigned int light = 0;
unsigned int setSoilMoisture = 10;
unsigned int soilMoisture;            // 土壤湿度
unsigned char setLightValue = 20;     // 光照设置值

bool shuaxin = 0;
bool shanshuo = 0;
bool sendFlag = 1;

// API配置
#define API_KEY "sk-drchipruirzifwonqwyazjxrvqailjdrahqgslmnlgcayzyk"
#define API_URL "https://api.siliconflow.cn/v1/chat/completions"
#define MODEL_NAME "deepseek-ai/DeepSeek-R1"

// API响应缓冲区
char apiResponseBuffer[1024];

// 函数声明
void InitDisplay(void);
void displayDHT11TempAndHumi(void);
void displayLight(void);
void displaySoilMoisture(void);
void displaySetValue(void);
void keyscan(void);
void UsartSendReceiveData(void);
bool getAIDecision(const char* prompt, char* response, size_t responseSize);

void InitDisplay(void)   // 初始化显示
{
    unsigned char i = 0;
    for(i = 0; i < 4; i++) OLED_ShowCN(i * 16, 0, i + 0, 0);  // 显示中文：环境温度
    for(i = 0; i < 4; i++) OLED_ShowCN(i * 16, 2, i + 4, 0);  // 显示中文：环境湿度
    for(i = 0; i < 4; i++) OLED_ShowCN(i * 16, 4, i + 8, 0);  // 显示中文：土壤湿度
    for(i = 0; i < 4; i++) OLED_ShowCN(i * 16, 6, i + 12, 0); // 显示中文：光照强度
    OLED_ShowChar(64, 0, ':', 2, 0);
    OLED_ShowChar(64, 2, ':', 2, 0);
    OLED_ShowChar(64, 4, ':', 2, 0);
    OLED_ShowChar(64, 6, ':', 2, 0);
}

void displayDHT11TempAndHumi(void)  // 显示环境温湿度
{
    DHT11_Read_Data(&temperature, &humidity);
    if(temperature >= setTempValue && shanshuo)
    {
        OLED_ShowChar(80, 0, ' ', 2, 0);
        OLED_ShowChar(88, 0, ' ', 2, 0);
    }
    else
    {
        OLED_ShowChar(80, 0, temperature / 10 + '0', 2, 0);
        OLED_ShowChar(88, 0, temperature % 10 + '0', 2, 0);
    }
    OLED_ShowCentigrade(96, 0);
    OLED_ShowChar(80, 2, humidity / 10 + '0', 2, 0);
    OLED_ShowChar(88, 2, humidity % 10 + '0', 2, 0);
    OLED_ShowChar(96, 2, '%', 2, 0);
}

void displayLight(void)  // 显示光照强度
{
    u16 test_adc = 0;
    
    // 获取光照值
    test_adc = Get_Adc_Average(ADC_Channel_9, 10);  // 读取通道9的10次AD平均值
    light = test_adc * 99 / 4096;  // 转换成0-99百分比
    light = light >= 99 ? 99 : light;  // 最大只能到百分之99
    if(light <= setLightValue && shanshuo)
    {
        OLED_ShowChar(80, 6, ' ', 2, 0);
        OLED_ShowChar(88, 6, ' ', 2, 0);
    }
    else
    {
        OLED_ShowChar(80, 6, light / 10 + '0', 2, 0);
        OLED_ShowChar(88, 6, light % 10 + '0', 2, 0);
    }
    OLED_ShowChar(96, 6, '%', 2, 0);
}

void displaySoilMoisture(void)  // 显示土壤湿度
{
    soilMoisture = 100 - (Get_Adc_Average(ADC_Channel_8, 10) * 99 / 4096);
    if(soilMoisture > 99) soilMoisture = 99;
    if(soilMoisture <= setSoilMoisture && shanshuo)
    {
        OLED_ShowChar(80, 4, ' ', 2, 0);
        OLED_ShowChar(88, 4, ' ', 2, 0);
    }
    else
    {
        OLED_ShowChar(80, 4, soilMoisture / 10 + '0', 2, 0);
        OLED_ShowChar(88, 4, soilMoisture % 10 + '0', 2, 0);
    }
    OLED_ShowChar(96, 4, '%', 2, 0);
}

void displaySetValue(void)  // 显示设置的值
{
    if(setn == 1)
    {
        OLED_ShowChar(60, 4, setSoilMoisture / 10 + '0', 2, 0);
        OLED_ShowChar(68, 4, setSoilMoisture % 10 + '0', 2, 0);
        OLED_ShowChar(76, 4, '%', 2, 0);
    }
    if(setn == 2)
    {
        OLED_ShowChar(60, 4, setTempValue / 10 + '0', 2, 0);
        OLED_ShowChar(68, 4, setTempValue % 10 + '0', 2, 0);
        OLED_ShowCentigrade(76, 4);
    }
    if(setn == 3)
    {
        OLED_ShowChar(60, 4, setLightValue / 10 + '0', 2, 0);
        OLED_ShowChar(68, 4, setLightValue % 10 + '0', 2, 0);
        OLED_ShowChar(76, 4, '%', 2, 0);
    }
}

void keyscan(void)   // 按键扫描
{
    u8 i;
    
    if(KEY1 == 0)  // 设置
    {
        delay_ms(20);
        if(KEY1 == 0)
        {
            while(KEY1 == 0);
            BEEP = 0;
            setn++;
            if(setn == 1)
            {
                OLED_CLS();    // 清屏
                for(i = 0; i < 2; i++)
                {
                    OLED_ShowCN(i * 16 + 16, 0, i + 16, 0);  // 显示"设置"
                }
                for(i = 0; i < 4; i++) OLED_ShowCN(i * 16 + 48, 0, i + 8, 0);  // 显示中文：土壤湿度
            }
            if(setn == 2)
            {
                for(i = 0; i < 4; i++) OLED_ShowCN(i * 16 + 48, 0, i + 0, 0);  // 显示中文：环境温度
            }
            if(setn == 3)
            {
                for(i = 0; i < 4; i++) OLED_ShowCN(i * 16 + 48, 0, i + 12, 0);  // 显示中文：光照强度
                OLED_ShowChar(84, 4, ' ', 2, 0);
            }
            displaySetValue();
            if(setn > 3)
            {
                setn = 0;
                OLED_CLS();    // 清屏
                InitDisplay();
            }
        }
    }
    if(KEY2 == 0)  // 增加
    {
        delay_ms(20);
        if(KEY2 == 0)
        {
            while(KEY2 == 0);
            if(setn == 1)
            {
                if(setSoilMoisture < 99) setSoilMoisture++;
            }
            if(setn == 2)
            {
                if(setTempValue < 99) setTempValue++;
            }
            if(setn == 3)
            {
                if(setLightValue < 99) setLightValue++;
            }
            displaySetValue();
        }
    }
    if(KEY3 == 0)  // 减少
    {
        delay_ms(20);
        if(KEY3 == 0)
        {
            while(KEY3 == 0);
            if(setn == 1)
            {
                if(setSoilMoisture > 0) setSoilMoisture--;
            }
            if(setn == 2)
            {
                if(setTempValue > 0) setTempValue--;
            }
            if(setn == 3)
            {
                if(setLightValue > 0) setLightValue--;
            }
            displaySetValue();
        }
    }
}

// 调用DeepSeek-R1 API进行决策的函数
bool getAIDecision(const char* prompt, char* response, size_t responseSize) {
    // 创建API请求的JSON载荷
    char requestPayload[512];
    snprintf(requestPayload, sizeof(requestPayload), 
             "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}", 
             MODEL_NAME, prompt);
    
    // 初始化HTTP客户端
    HttpClient client;
    HttpClientInit(&client, API_URL);
    
    // 设置请求头
    HttpClientSetHeader(&client, "Content-Type: application/json");
    HttpClientSetHeader(&client, "Authorization: Bearer %s", API_KEY);
    
    // 发送POST请求
    HttpResponse httpResponse;
    if (!HttpClientPost(&client, requestPayload, &httpResponse)) {
        return false;
    }
    
    // 解析响应JSON以提取决策
    // 这是一个简化的占位符 - 你需要适当的JSON解析
    // 从response.choices[0].message.content中提取内容
    if (ParseJsonResponse(httpResponse.body, response, responseSize)) {
        HttpClientCleanup(&client);
        return true;
    }
    
    HttpClientCleanup(&client);
    return false;
}

void UsartSendReceiveData(void)
{
    unsigned char *dataPtr = NULL;
    char *str1 = 0, i;
    int setValue = 0;
    char setvalue[5] = {0};
    char SEND_BUF[100];
    
    dataPtr = ESP8266_GetIPD(0);   // 接收数据
    if(dataPtr != NULL)
    {
        if(strstr((char *)dataPtr, "light:") != NULL)
        {
            BEEP = 1;
            delay_ms(80);
            BEEP = 0;
            
            str1 = strstr((char *)dataPtr, "light:");
            
            while(*str1 < '0' || *str1 > '9')    // 判断是不是0到9有效数字
            {
                str1 = str1 + 1;
            }
            i = 0;
            while(*str1 >= '0' && *str1 <= '9')  // 判断是不是0到9有效数字
            {
                setvalue[i] = *str1;
                i++; str1++;
            }
            setValue = atoi(setvalue);
            setLightValue = setValue;    // 设置的光照值
            displaySetValue();
        }
        
        if(strstr((char *)dataPtr, "temp:") != NULL)
        {
            str1 = strstr((char *)dataPtr, "temp:");
            
            while(*str1 < '0' || *str1 > '9')    // 判断是不是0到9有效数字
            {
                str1 = str1 + 1;
            }
            i = 0;
            while(*str1 >= '0' && *str1 <= '9')  // 判断是不是0到9有效数字
            {
                setvalue[i] = *str1;
                i++; str1++;
            }
            setValue = atoi(setvalue);
            setTempValue = setValue;    // 设置的温度值
            displaySetValue();
        }
        
        if(strstr((char *)dataPtr, "soil_hmd:") != NULL)
        {
            str1 = strstr((char *)dataPtr, "soil_hmd:");
            
            while(*str1 < '0' || *str1 > '9')    // 判断是不是0到9有效数字
            {
                str1 = str1 + 1;
            }
            i = 0;
            while(*str1 >= '0' && *str1 <= '9')  // 判断是不是0到9有效数字
            {
                setvalue[i] = *str1;
                i++; str1++;
            }
            setValue = atoi(setvalue);
            setSoilMoisture = setValue;    // 设置的土壤湿度值
            displaySetValue();
        }
    }
    if(sendFlag == 1)    // 1秒钟上传一次数据
    {
        sendFlag = 0;
        
        memset(SEND_BUF, 0, sizeof(SEND_BUF));   // 清空缓冲区
        sprintf(SEND_BUF, "$temp:%d#,$humi:%d#,$light:%d#,$soil_hmd:%d#", temperature, humidity, light, soilMoisture);
        ESP8266_SendData((u8 *)SEND_BUF, strlen(SEND_BUF));
    }
}

// 基于AI的设备控制决策函数
void makeAIDecisions() {
    char prompt[256];
    char aiResponse[128];
    bool lightOn = false, motorOn = false, fanOn = false, beepOn = false;
    
    // 创建包含当前传感器数据的提示
    snprintf(prompt, sizeof(prompt), 
             "根据以下智能农业数据，决定哪些设备应该打开：\n"
             "温度：%d°C (阈值：%d°C)\n"
             "湿度：%d%%\n"
             "土壤湿度：%d%% (阈值：%d%%)\n"
             "光照水平：%d%% (阈值：%d%%)\n"
             "只需回复：light,motor,fan,beep（逗号分隔，只包含应该开启的设备）",
             temperature, setTempValue, humidity, soilMoisture, setSoilMoisture, light, setLightValue);
    
    // 从AI获取决策
    if (getAIDecision(prompt, aiResponse, sizeof(aiResponse))) {
        // 解析响应以确定哪些设备应该开启
        if (strstr(aiResponse, "light")) lightOn = true;
        if (strstr(aiResponse, "motor")) motorOn = true;
        if (strstr(aiResponse, "fan")) fanOn = true;
        if (strstr(aiResponse, "beep")) beepOn = true;
        
        // 应用决策
        LED = lightOn ? 1 : 0;
        MOTOR = motorOn ? 1 : 0;
        RELAY = fanOn ? 1 : 0;
        BEEP = beepOn ? 1 : 0;
    } else {
        // 如果AI决策失败，回退到原始逻辑
        if (light <= setLightValue) LED = 1; else LED = 0;   // 光线暗开，光线强关
        if (soilMoisture <= setSoilMoisture) MOTOR = 1; else MOTOR = 0;  // 开启水泵
        if (temperature >= setTempValue) RELAY = 1; else RELAY = 0;   // 开风扇
        if (light <= setLightValue || soilMoisture <= setSoilMoisture || temperature >= setTempValue) BEEP = 1; else BEEP = 0;
    }
}

int main(void)
{
    delay_init();             // 延时函数初始化  
    NVIC_Configuration();     // 中断优先级配置
    I2C_Configuration();      // IIC初始化
    OLED_Init();              // OLED液晶初始化
    Adc_Init();
    OLED_CLS();               // 清屏
    KEY_GPIO_Init();          // 按键引脚初始化    
    OLED_ShowStr(0, 2, "   loading...   ", 2, 0);  // 显示"加载中..."
    
    // 初始化HTTP客户端库
    HttpClientLibInit();
    
    ESP8266_Init();           // ESP8266初始化
    MOTOR_GPIO_Init();
    while(DHT11_Init())
    {
        OLED_ShowStr(0, 2, "  DHT11 Error!  ", 2, 0);  // 显示"DHT11错误！"
        delay_ms(500);
    }
    OLED_CLS();               // 清屏
    InitDisplay();
    TIM3_Init(99, 719);       // 定时器初始化，定时1ms
    
    while(1)
    { 
        keyscan();            // 按键扫描
        
        if(setn == 0)         // 不在设置状态下
        {
            if(shuaxin == 1)  // 大约500ms刷新一次
            { 
                shuaxin = 0;
                displayDHT11TempAndHumi();
                displaySoilMoisture();
                displayLight();
                
                // 使用基于AI的决策代替硬编码逻辑
                makeAIDecisions();
            }
        }
        UsartSendReceiveData();   // 串口发送接收数据
        delay_ms(20);
    }
}

void TIM3_IRQHandler(void)  // 定时器3中断服务程序，用于记录时间
{ 
    static u16 timeCount1 = 0;
    static u16 timeCount2 = 0;
    
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)  // 检查指定的TIM中断是否发生 
    { 
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);     // 清除中断标志位  

        timeCount1++;
        timeCount2++;
        if(timeCount1 >= 300)  // 300ms
        {
            timeCount1 = 0;
            shanshuo = !shanshuo;
            shuaxin = 1;
        }
        if(timeCount2 >= 800)  // 800ms
        {
            timeCount2 = 0;
            sendFlag = 1;
        }
    }
}
