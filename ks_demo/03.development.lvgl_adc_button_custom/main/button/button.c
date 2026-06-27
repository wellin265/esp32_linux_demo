/*
 * @Descripttion : 
 * @version      : 
 * @Author       : Kevincoooool
 * @Date         : 2021-03-25 17:15:11
 * @LastEditors: Kevincoooool 33611679+Kevincoooool@users.noreply.github.com
 * @LastEditTime: 2024-12-05 12:56:36
 * @FilePath: \Korvo_Demo\10.adc_button_test\main\button\button.c
 */

#include "button.h"
#include "bsp_adc.h"
/*******************************************************************
 *                          ��������                               
 *******************************************************************/

Button_t Button_UP;
Button_t Button_ENTER;
Button_t Button_DOWN;
Button_t Button_RIGHT;
Button_t Button_LEFT;
Button_t Button_SIX;
uint8_t Button_Value = BT_NONE;
char * Button_Tips[]={
	"No Button!\n",
	"Button 1 down!\n",
	"Button 1 Double!\n",
	"Button 1 Long!\n",
	"Button 1 Long_Free!\n",
	
	"Button 2 down!\n",
	"Button 2 Double!\n",
	"Button 2 Long!\n",
	"Button 2 Long_Free!\n",

	"Button 3 down!\n",
	"Button 3 Double!\n",
	"Button 3 Long!\n",
	"Button 3 Long_Free!\n",

	"Button 4 down!\n",
	"Button 4 Double!\n",
	"Button 4 Long!\n",
	"Button 4 Long_Free!\n",

	"Button 5 down!\n",
	"Button 5 Double!\n",
	"Button 5 Long!\n",
	"Button 5 Long_Free!\n",

    "Button 6 down!\n",
	"Button 6 Double!\n",
	"Button 6 Long!\n",
	"Button 6 Long_Free!\n",
};
//����һ����
void Btn1_Down_CallBack(void *btn)
{

	Button_Value = BT1_DOWN;
	printf("Button 1 down!\n");

}
//����һ˫��
void Btn1_Double_CallBack(void *btn)
{
	Button_Value = BT1_DOUBLE;
	printf("Button 1 Double!\n");
}
//����һ˫��
void Btn1_Long_CallBack(void *btn)
{
	Button_Value = BT1_LONG;
	printf("Button 1 Long!\n");
}
void Btn1_Long_Free_CallBack(void *btn)
{
	Button_Value = BT1_LONGFREE;
	printf("Button 1 Long_Free!\n");
}
///////////////////////////////////////////////////////
//����������
void Btn2_Down_CallBack(void *btn)
{
	Button_Value = BT2_DOWN;
	printf("Button 2 down!\n");
}
//����һ˫��
void Btn2_Double_CallBack(void *btn)
{
	Button_Value = BT2_DOUBLE;
	printf("Button 2 Double!\n");
}
//����һ˫��
void Btn2_Long_CallBack(void *btn)
{
	Button_Value = BT2_LONG;
	printf("Button 2Long!\n");
}
void Btn2_Long_Free_CallBack(void *btn)
{
	Button_Value = BT2_LONGFREE;
	printf("Button 2 Long_Free!\n");
}
///////////////////////////////////////////////////////
//����������
void Btn3_Down_CallBack(void *btn)
{
	Button_Value = BT3_DOWN;
	printf("Button 3 down!\n");
}
//����һ˫��
void Btn3_Double_CallBack(void *btn)
{
	Button_Value = BT3_DOUBLE;
	printf("Button 3 Double!\n");
}
//����һ˫��
void Btn3_Long_CallBack(void *btn)
{
	Button_Value = BT3_LONG;
	printf("Button 3 Long!\n");
}
void Btn3_Long_Free_CallBack(void *btn)
{
	Button_Value = BT3_LONGFREE;
	printf("Button 3 Long_Free!\n");
}

///////////////////////////////////////////////////////
//����������
void Btn4_Down_CallBack(void *btn)
{
	Button_Value = BT4_DOWN;
	printf("Button 4 down!\n");
}
//����һ˫��
void Btn4_Double_CallBack(void *btn)
{
	Button_Value = BT4_DOUBLE;
	printf("Button 4 Double!\n");
}
//����һ˫��
void Btn4_Long_CallBack(void *btn)
{
	Button_Value = BT4_LONG;
	printf("Button 4 Long!\n");
}
void Btn4_Long_Free_CallBack(void *btn)
{
	Button_Value = BT4_LONGFREE;
	printf("Button 4 Long_Free!\n");
}
///////////////////////////////////////////////////////
//����������
void Btn5_Down_CallBack(void *btn)
{
	Button_Value = BT5_DOWN;
	printf("Button 5 down!\n");
}
//����һ˫��
void Btn5_Double_CallBack(void *btn)
{
	Button_Value = BT5_DOUBLE;
	printf("Button 5 Double!\n");
}
//����һ˫��
void Btn5_Long_CallBack(void *btn)
{
	Button_Value = BT5_LONG;
	printf("Button 5 Long!\n");
}
void Btn5_Long_Free_CallBack(void *btn)
{
	Button_Value = BT5_LONGFREE;
	printf("Button 5 Long_Free!\n");
}
///////////////////////////////////////////////////////
//����������
void Btn6_Down_CallBack(void *btn)
{
	Button_Value = BT6_DOWN;
	printf("Button 6 down!\n");
}
//����һ˫��
void Btn6_Double_CallBack(void *btn)
{
	Button_Value = BT6_DOUBLE;
	printf("Button 6 Double!\n");
}
//����һ˫��
void Btn6_Long_CallBack(void *btn)
{
	Button_Value = BT6_LONG;
	printf("Button 6 Long!\n");
}
void Btn6_Long_Free_CallBack(void *btn)
{
	Button_Value = BT6_LONGFREE;
	printf("Button 6 Long_Free!\n");
}
uint8_t Read_KEY1_Level(void)
{
	return (Get_Adc_Button() == 1) ? 1 : 0;
}

uint8_t Read_KEY2_Level(void)
{
	return (Get_Adc_Button() == 2) ? 1 : 0;
}
uint8_t Read_KEY3_Level(void)
{
	return (Get_Adc_Button() == 3) ? 1 : 0;
}
uint8_t Read_KEY4_Level(void)
{
	return (Get_Adc_Button() == 4) ? 1 : 0;
}
uint8_t Read_KEY5_Level(void)
{
	return (Get_Adc_Button() == 5) ? 1 : 0;
}
uint8_t Read_KEY6_Level(void)
{
	return (Get_Adc_Button() == 6) ? 1 : 0;
}
void Button_Init(void)
{

	Button_Create("Button_UP",
				  &Button_UP,
				  Read_KEY1_Level,
				  KEY_ON);
	Button_Attach(&Button_UP, BUTTON_DOWN, Btn1_Down_CallBack);			//����
	Button_Attach(&Button_UP, BUTTON_DOUBLE, Btn1_Double_CallBack);		//˫��
	Button_Attach(&Button_UP, BUTTON_LONG, Btn1_Long_CallBack);			//��������
	Button_Attach(&Button_UP, BUTTON_LONG_FREE, Btn1_Long_Free_CallBack); //�����ɿ�

	Button_Create("Button_ENTER",
				  &Button_ENTER,
				  Read_KEY2_Level,
				  KEY_ON);
	Button_Attach(&Button_ENTER, BUTTON_DOWN, Btn2_Down_CallBack);			//����
	Button_Attach(&Button_ENTER, BUTTON_DOUBLE, Btn2_Double_CallBack);		//˫��
	Button_Attach(&Button_ENTER, BUTTON_LONG, Btn2_Long_CallBack);			//��������
	Button_Attach(&Button_ENTER, BUTTON_LONG_FREE, Btn2_Long_Free_CallBack); //�����ɿ�

	Button_Create("Button_DOWN",
				  &Button_DOWN,
				  Read_KEY3_Level,
				  KEY_ON);
	Button_Attach(&Button_DOWN, BUTTON_DOWN, Btn3_Down_CallBack);			//����
	Button_Attach(&Button_DOWN, BUTTON_DOUBLE, Btn3_Double_CallBack);		//˫��
	Button_Attach(&Button_DOWN, BUTTON_LONG, Btn3_Long_CallBack);			//��������
	Button_Attach(&Button_DOWN, BUTTON_LONG_FREE, Btn3_Long_Free_CallBack); //�����ɿ�

	Button_Create("Button_RIGHT",
				  &Button_RIGHT,
				  Read_KEY4_Level,
				  KEY_ON);
	Button_Attach(&Button_RIGHT, BUTTON_DOWN, Btn4_Down_CallBack);			//����
	Button_Attach(&Button_RIGHT, BUTTON_DOUBLE, Btn4_Double_CallBack);		//˫��
	Button_Attach(&Button_RIGHT, BUTTON_LONG, Btn4_Long_CallBack);			//��������
	Button_Attach(&Button_RIGHT, BUTTON_LONG_FREE, Btn4_Long_Free_CallBack); //�����ɿ�


	Button_Create("Button_LEFT",
				  &Button_LEFT,
				  Read_KEY5_Level,
				  KEY_ON);
	Button_Attach(&Button_LEFT, BUTTON_DOWN, Btn5_Down_CallBack);			//����
	Button_Attach(&Button_LEFT, BUTTON_DOUBLE, Btn5_Double_CallBack);		//˫��
	Button_Attach(&Button_LEFT, BUTTON_LONG, Btn5_Long_CallBack);			//��������
	Button_Attach(&Button_LEFT, BUTTON_LONG_FREE, Btn5_Long_Free_CallBack); //�����ɿ�
	Button_Create("Button_SIX",
				  &Button_SIX,
				  Read_KEY6_Level,
				  KEY_ON);
	Button_Attach(&Button_SIX, BUTTON_DOWN, Btn6_Down_CallBack);			//����
	Button_Attach(&Button_SIX, BUTTON_DOUBLE, Btn6_Double_CallBack);		//˫��
	Button_Attach(&Button_SIX, BUTTON_LONG, Btn6_Long_CallBack);			//��������
	Button_Attach(&Button_SIX, BUTTON_LONG_FREE, Btn6_Long_Free_CallBack); //�����ɿ�


	Get_Button_Event(&Button_UP);
	Get_Button_Event(&Button_ENTER);
	Get_Button_Event(&Button_DOWN);
    	Get_Button_Event(&Button_SIX);

    
}
