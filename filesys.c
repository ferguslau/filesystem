// File Name    : filesys.c
// Project      : Simple File System by SPI
// Description  : This project aims to perform a simple file system that supports 
//                create, read, write, list and delete functions by connecting 
//                two stm32F411 board using SPI where one devices acts a master 
//                and the other as slave

#include "common.h"
#include <stdio.h>

//SPI1 - Master
//PB3 SCK
//PA6 MISO
//PA7 MOSI
//
//SPI2 - Slave
//PB10 SCK
//PB14 MISO
//PB15 MOSI


#define MAX_FILE_NUMBER 100	// file_number: 1 to 100
#define MAX_FILE_SIZE 100	// max 100 byte in each file


volatile uint8_t rxData1 = 0;
volatile uint8_t rxData2 = 0;
volatile uint8_t rxData1_f = 0;
volatile uint8_t rxData2_f = 0;

enum state {SYNC, CMD, LIST, CREATE, WRITE, READ, DELETE};

volatile enum state current_state = SYNC;

struct file_record 
{
	uint8_t size;
	uint8_t data[MAX_FILE_SIZE];
};

volatile struct file_record file[MAX_FILE_NUMBER + 1] = {0};
volatile uint8_t file_number = 0;
volatile uint8_t flag_filename = 0;
volatile uint8_t flag_rx_count = 0;
volatile uint8_t create_file_number = 0;
volatile uint8_t write_file_number = 0;
volatile uint8_t write_count = 0;
volatile uint8_t read_file_number = 0;
volatile uint8_t read_count = 0;


void spi_init(void) {
        //Enable the clock for GPIOA
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        //Enable the clock for GPIOB
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

	//Reset SPI1 peripheral
        RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
        RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
        RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;

        //Reset SPI2 peripheral
        RCC->APB1RSTR |= RCC_APB1RSTR_SPI2RST;
        RCC->APB1RSTR |= RCC_APB1RSTR_SPI2RST;
        RCC->APB1RSTR &= ~RCC_APB1RSTR_SPI2RST;

        //Enable SPI1 clock
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

        //Enable SPI2 clock
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;


        //Initialize GPIO to AF for SPI1
        GPIOB->MODER |= GPIO_MODER_MODER3_1;
        GPIOA->MODER |= GPIO_MODER_MODER6_1;
        GPIOA->MODER |= GPIO_MODER_MODER7_1;

        //Initialize GPIO to AF for SPI2
        GPIOB->MODER |= GPIO_MODER_MODER10_1;
        GPIOB->MODER |= GPIO_MODER_MODER14_1;
        GPIOB->MODER |= GPIO_MODER_MODER15_1;


        //SPI1: GPIO set speed to the highest
        GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR3;
        GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6;
        GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR7;

        //SPI2: GPIO set speed to the highest
        GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR10;
        GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR14;
        GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR15;

        //SPI1: set GPIO AF tpye to AF05
        GPIOB->AFR[0] |= GPIO_AFRL_AFRL3_0 | GPIO_AFRL_AFRL3_2;
        GPIOA->AFR[0] |= GPIO_AFRL_AFRL6_0 | GPIO_AFRL_AFRL6_2;
        GPIOA->AFR[0] |= GPIO_AFRL_AFRL7_0 | GPIO_AFRL_AFRL7_2;

        //SPI2: set GPIO AF type to AF05
        GPIOB->AFR[1] |= GPIO_AFRH_AFRH2_0 | GPIO_AFRH_AFRH2_2;
        GPIOB->AFR[1] |= GPIO_AFRH_AFRH6_0 | GPIO_AFRH_AFRH6_2;
        GPIOB->AFR[1] |= GPIO_AFRH_AFRH7_0 | GPIO_AFRH_AFRH7_2;


// SPI1 configuration:

        //Data frame format 8-bit
        SPI1->CR1 &= ~SPI_CR1_DFF;
        //Clock low while idling
        SPI1->CR1 &= ~SPI_CR1_CPOL;
        //Capture at the rising edge
        SPI1->CR1 &= ~SPI_CR1_CPHA;
        //LSB first
        SPI1->CR1 |= SPI_CR1_LSBFIRST;
        //Baudrate 100MHz/256
        SPI1->CR1 |= SPI_CR1_BR_1 | SPI_CR1_BR_2 | SPI_CR1_BR_0;

        //Use software to control the NSS
        //This is for slave
        //SPI->CR1 |= SPI_CR1_SSM;

        //This is for master
        SPI1->CR1 |= SPI_CR1_SSM;
        SPI1->CR1 |= SPI_CR1_SSI;
        //Select Master
        SPI1->CR1 |= SPI_CR1_MSTR;

        //Full duplex
        SPI1->CR1 &= ~SPI_CR1_BIDIMODE;
        SPI1->CR1 &= ~SPI_CR1_RXONLY;

        //Enable SPI1 Rx interrupt
        SPI1->CR2 |= SPI_CR2_RXNEIE;

        //Enable SPI1 interrupt
        NVIC_EnableIRQ(SPI1_IRQn);

        //Enable SPI1
        SPI1->CR1 |= SPI_CR1_SPE;




// SPI2 configuration:

        //Data frame format 8-bit
        SPI2->CR1 &= ~SPI_CR1_DFF;
        //Clock low while idling
        SPI2->CR1 &= ~SPI_CR1_CPOL;
        //Capture at the rising edge
        SPI2->CR1 &= ~SPI_CR1_CPHA;
        //LSB first
        SPI2->CR1 |= SPI_CR1_LSBFIRST;
        //Baudrate 100MHz/256
        SPI2->CR1 |= SPI_CR1_BR_1 | SPI_CR1_BR_2 | SPI_CR1_BR_0;

        //Use software to control the NSS
        //This is for slave
        SPI2->CR1 |= SPI_CR1_SSM;

        //This is for master
 //       SPI2->CR1 |= SPI_CR1_SSM;
 //       SPI2->CR1 |= SPI_CR1_SSI;

	 //Select Slave
        SPI2->CR1 &= ~SPI_CR1_MSTR;

        //Full duplex
        SPI2->CR1 &= ~SPI_CR1_BIDIMODE;
        SPI2->CR1 &= ~SPI_CR1_RXONLY;

        //Enable SPI2 Rx interrupt
        SPI2->CR2 |= SPI_CR2_RXNEIE;

        //Enable SPI2 interrupt
        NVIC_EnableIRQ(SPI2_IRQn);

        //Enable SPI2
        SPI2->CR1 |= SPI_CR1_SPE;


}
	

void SPI1_IRQHandler(void)
{
	if (SPI1->SR & SPI_SR_RXNE) {
		rxData1 = SPI1->DR;
		rxData1_f = 1;
	}
}


void SPI2_IRQHandler(void)
{

        if (SPI2->SR & SPI_SR_RXNE) {
                      rxData2 = SPI2->DR;
                      rxData2_f = 1;


		switch(current_state)
		{
			case SYNC:
				if (rxData2 == 0xfe) {
					current_state = CMD;
				}
				break;
		
			case CMD:
				if (rxData2 == 0x00) {
					current_state = LIST;
					SPI2->DR = 1;		// ACK
					file_number = 0;
					flag_filename = 1;
				}
				else if (rxData2 == 0x03) {
					current_state = CREATE;
					SPI2->DR = 1;		// ACK
					flag_rx_count = 0;
				}
				else if (rxData2 == 0x02) {
					current_state = WRITE;
					SPI2->DR = 1;		// ACK
					flag_rx_count = 0;
					write_count = 0;
				}
				else if (rxData2 == 0x01) {
	                                current_state = READ;
	                                SPI2->DR = 1;           // ACK
	                                flag_rx_count = 0;
	                                read_count = 0;
	                        }
	                        else if (rxData2 == 0x04) {
	                                current_state = DELETE;
	                                SPI2->DR = 1;           // ACK
	                                flag_rx_count = 0;
	                        }
				else {
					current_state = SYNC;
				}
				break;
		
			case LIST:
				if (flag_filename == 1) {
					if (file_number > MAX_FILE_NUMBER) {
						SPI2->DR = 0;	   	// NACK
					   	current_state = SYNC;	
					} 
					else {
						while (file[file_number].size == 0) {
							file_number++;
							if (file_number > MAX_FILE_NUMBER) {
								break;
							}
						}
	
						if (file_number > MAX_FILE_NUMBER) {
							SPI2->DR = 0;		// NACK
						   	current_state = SYNC;	
						}
						else {
							SPI2->DR = file_number;	// file number
						   	flag_filename = 0;
						}
					   }
				}
				else {
					   SPI2->DR = file[file_number].size;
					   flag_filename = 1;
					   file_number++;
				}
				break;

			case CREATE:
				if (flag_rx_count == 0) {
					flag_rx_count = 1;
					SPI2->DR = 1;			// ACK
				}
				else if (flag_rx_count == 1) {
					create_file_number = rxData2;
					flag_rx_count = 2;
					SPI2->DR = 1;			// ACK
				}
				else if (flag_rx_count == 2) {
					file[create_file_number].size = rxData2;
					flag_rx_count = 3;
					current_state = SYNC;
				}	

				break;	


			case WRITE:
	                        if (flag_rx_count == 0) {
	                                flag_rx_count = 1;		// skip this dummy data
	                        }
	                        else if (flag_rx_count == 1) {
	                                write_file_number = rxData2;	// receive the file number
	                                flag_rx_count = 2;
                                
	                        }
	                        else if (flag_rx_count == 2) {
	                                if (file[write_file_number].size == 1) {
						flag_rx_count = 4;
						SPI2->DR = 1;		// ACK
					}
					else {
						flag_rx_count = 3;		// skip this dummy data
					}
				}
				else if (flag_rx_count == 3) {
                                
					file[write_file_number].data[write_count] = rxData2;	// receive data
	                                write_count++;
					if (write_count == file[write_file_number].size - 1) {
						flag_rx_count = 4;
						SPI2->DR = 1;		// ACK
					}
	                        }
	                        else if (flag_rx_count == 4) {
	                                file[write_file_number].data[write_count] = rxData2;
	                                current_state = SYNC;
	                        }

	                        break;

			case READ:
	                        if (flag_rx_count == 0) {
	                                flag_rx_count = 1;              // skip this dummy data
	                        }
	                        else if (flag_rx_count == 1) {
	                                read_file_number = rxData2;    // receive the file number
	                                SPI2->DR = 1;			// ACK
					flag_rx_count = 2;
	                        }
	                        else if (flag_rx_count == 2) {
					SPI2->DR = file[read_file_number].data[read_count];
					read_count++;
					if (read_count == file[read_file_number].size) {
						current_state = SYNC;
					}
				}

				break;

	 		case DELETE:
	                        if (flag_rx_count == 0) {
	                                flag_rx_count = 1;              // skip this dummy data
	                        }
	                        else if (flag_rx_count == 1) {
	                                file[rxData2].size = 0;		// receive the file number
	                                SPI2->DR = 1;                   // ACK
	                                current_state = SYNC;
	                        }

				break;

			default:
				current_state = SYNC;
			
		}
	}
}



void spi_write(uint8_t * data, uint8_t length)
{
	uint32_t i;
	
	for (i = 0; i < 100000; i++) {
		while (!(SPI1->SR & SPI_SR_TXE));
		SPI1->DR = 170;
	}
	while (SPI1->SR & SPI_SR_BSY);
}

void spi_write_spi2(uint8_t * data, uint8_t length)
{
        uint32_t i;

        for (i = 0; i < 100000; i++) {
                while (!(SPI2->SR & SPI_SR_TXE));
                SPI2->DR = 170;
        }
        while (SPI2->SR & SPI_SR_BSY);
}


void spi_write_1B(uint8_t * data)
{
        rxData1_f = 0;
	rxData2_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = data[0];
//	SPI2->DR = data[0];        
        while (SPI1->SR & SPI_SR_BSY);

	HAL_Delay(200);

	if (rxData1_f == 1) {
		printf("M: %d\n", rxData1);
		rxData1_f = 0;
	}

	if (rxData2_f == 1) {
		printf("S: %d\n", rxData2);
		rxData2_f = 0;
	}


}


void create(uint8_t file_number, uint8_t file_size)
{
        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xfe;                        // SYNC
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0x03;                        // CREATE
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xff;                        // 0xff
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        if (rxData1_f == 1 && rxData1 == 1) {

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = file_number;                     // file name
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = file_size;                       // file size
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

		rxData1_f = 0;
        }
}


void delete(uint8_t file_number)
{
        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xfe;                        // SYNC
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0x04;                        // DELETE
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xff;                        // 0xff
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        if (rxData1_f == 1 && rxData1 == 1) {

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = file_number;                       // file number 
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

		rxData1_f = 0;

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = 0xff;                           // send dummy byte 0xff
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

                if (rxData1_f != 1 || rxData1 != 1) {
                        printf("master Delete: No ACK received after sending file number");
                }

		rxData1_f = 0;
        }
        else {
                printf("Delete error! \n\n");
        }
}



void read(uint8_t file_number, uint8_t file_size)
{
        uint8_t i = 0;

        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xfe;                        // SYNC
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0x01;                        // READ
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xff;                        // 0xff
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        if (rxData1_f == 1 && rxData1 == 1) {

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = file_number;                       // file number 
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

                rxData1_f = 0;

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = 0xff;                           // send dummy byte 0xff
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

                if (rxData1_f != 1 || rxData1 != 1) {
                        printf("master Write: No ACK received after sending file number");
                }


                for (i = 0; i < file_size; i++) {
                        rxData1_f = 0;
                        while (!(SPI1->SR & SPI_SR_TXE));
                        SPI1->DR = 0xff;			// send 0xff
                        while (SPI1->SR & SPI_SR_BSY);
                        HAL_Delay(50);

			printf("%d   ", rxData1);
                }
		
		printf("\n\n");

                rxData1_f = 0;
        }
        else {
                printf("Read error! \n\n");
        }
}






void write(uint8_t para_num, uint32_t * para)
{
	uint8_t i = 0;

	rxData1_f = 0;

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xfe;                        // SYNC
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        rxData1_f = 0;
	
	while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0x02;                        // WRITE
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        rxData1_f = 0;

	while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xff;                        // 0xff
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        if (rxData1_f == 1 && rxData1 == 1) {

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = *para;                     	// file name 
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

		rxData1_f = 0;

                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = 0xff;                    	   // send dummy byte 0xff
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);

		if (rxData1_f != 1 || rxData1 != 1) {
			printf("master Write: No ACK received after sending file number");
		}	
		


		for (i = 1; i < para_num; i++) {
			rxData1_f = 0;
			while (!(SPI1->SR & SPI_SR_TXE));
                	SPI1->DR = (uint8_t)*(para + i);    // send data byte
                	while (SPI1->SR & SPI_SR_BSY);
                	HAL_Delay(50);
		}


		if (rxData1_f != 1 || rxData1 != 1) {
                        printf("master Write: No ACK received after data");
                }

                rxData1_f = 0;
        }
	else {
		printf("Write error! \n\n");
	}
}





void list()
{
        rxData1_f = 0;
       
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xfe;			// SYNC
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0x00;                        // LIST
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = 0xff;                        // 0xff
        while (SPI1->SR & SPI_SR_BSY);
        HAL_Delay(50);

        if (rxData1_f == 1 && rxData1 == 1) {
	        
volatile uint8_t kk = 0;

	do {
                while (!(SPI1->SR & SPI_SR_TXE));
                SPI1->DR = 0xff;                        // 0xff
                while (SPI1->SR & SPI_SR_BSY);
                HAL_Delay(50);
		if (rxData1 == 0) {
			printf("\n");
		}
		else {
			if (kk == 0) {
				printf("%d  ", rxData1);
				kk = 1;
			}
			else {
				printf("%d\n", rxData1);
				kk = 0;
			}
		}
	
	}
	while (rxData1 != 0);


		rxData1_f = 0;
        }

}


	
ParserReturnVal_t spiinit(int mode)
{
        if (mode != CMD_INTERACTIVE)
        return CmdReturnOk;

        spi_init();

        return CmdReturnOk;
}

ADD_CMD("spiinit", spiinit, "               Initialize the SPI")


ParserReturnVal_t CmdSpiWrite(int mode)
{
	uint32_t val = 0;
	fetch_uint32_arg(&val);
	spi_write((uint8_t *)&val, 1);
	
	return CmdReturnOk;
}

ADD_CMD("senddata", CmdSpiWrite,"   send data using SPI 1")

ParserReturnVal_t CmdSpiWrite_2(int mode)
{
        uint32_t val = 0;
        fetch_uint32_arg(&val);
        spi_write_spi2((uint8_t *)&val, 1);

        return CmdReturnOk;
}

ADD_CMD("senddata2", CmdSpiWrite_2,"   send data using SPI 2")


ParserReturnVal_t CmdSpiWrite_1B(int mode)
{
        uint32_t val = 0;
        fetch_uint32_arg(&val);
        spi_write_1B((uint8_t *)&val);

        return CmdReturnOk;
}

ADD_CMD("s1b", CmdSpiWrite_1B,"   send 1 byte data using SPI 1")

ParserReturnVal_t CmdList(int mode)
{

	list();
        
        return CmdReturnOk;
}

ADD_CMD("list", CmdList,"   send CMD LIST using SPI 1")




ParserReturnVal_t CmdCreate(int mode)
{
	uint32_t rc1, rc2;
        uint32_t file_number;
	uint32_t file_size;

        if (mode != CMD_INTERACTIVE)
        return CmdReturnOk;

        rc1 = fetch_uint32_arg(&file_number);
        if (rc1)
        {
                printf("Must specify the file number!\n");
                return CmdReturnBadParameter1;
        }

        rc2 = fetch_uint32_arg(&file_size);
        if (rc2)
        {
                printf("Must specify the file size!\n");
                return CmdReturnBadParameter2;
        }

        create((uint8_t)file_number, (uint8_t)file_size);

        return CmdReturnOk;
}

ADD_CMD("create", CmdCreate,"   send CMD CREATE using SPI 1")


ParserReturnVal_t CmdWrite(int mode)
{
        uint32_t rc;
	uint32_t para[100] = {0};
	uint8_t para_num = 0;

	if (mode != CMD_INTERACTIVE)
        return CmdReturnOk;

	do {
		rc = fetch_uint32_arg(para + para_num);
		para_num++;
	}
	while (rc == 0);

	write(para_num - 1, para);

        return CmdReturnOk;
}

ADD_CMD("write", CmdWrite,"   send Write command using SPI 1")



ParserReturnVal_t CmdRead(int mode)
{
        uint32_t rc1, rc2;
        uint32_t file_number;
        uint32_t file_size;

        if (mode != CMD_INTERACTIVE)
        return CmdReturnOk;

        rc1 = fetch_uint32_arg(&file_number);
        if (rc1)
        {
                printf("Must specify the file number!\n");
                return CmdReturnBadParameter1;
        }

        rc2 = fetch_uint32_arg(&file_size);
        if (rc2)
        {
                printf("Must specify the file size!\n");
                return CmdReturnBadParameter2;
        }

        read((uint8_t)file_number, (uint8_t)file_size);

        return CmdReturnOk;
}

ADD_CMD("read", CmdRead,"   send CMD READ using SPI 1")


ParserReturnVal_t CmdDelete(int mode)
{
        uint32_t rc;
        uint32_t file_number;
        

        if (mode != CMD_INTERACTIVE)
        return CmdReturnOk;

        rc = fetch_uint32_arg(&file_number);
        if (rc)
        {
                printf("Must specify the file number!\n");
                return CmdReturnBadParameter1;
        }

        delete((uint8_t)file_number);

        return CmdReturnOk;
}

ADD_CMD("delete", CmdDelete,"   send CMD DELETE using SPI 1")



