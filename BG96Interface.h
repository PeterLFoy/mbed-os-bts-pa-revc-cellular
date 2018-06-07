/* mbed Microcontroller Library
* Copyright (c) 20015 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
  ******************************************************************************
  * @file    BG96Interface.h 
  * @author  STMicroelectronics
  * @brief   Header file of the NetworkStack for the BG96 Device
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
  ******************************************************************************
  */
  
#ifndef BG96SA_INTERFACE_H
#define BG96SA_INTERFACE_H

#include "BG96.h"
 
#define BG96SA_SOCKET_COUNT 8
#define SERVER_SOCKET_NO    9
  
/** BG96SAInterface class
 *  Implementation of the NetworkStack for the BG96 Device
 */
class BG96Interface 
{
public:
    BG96Interface(PinName tx, PinName rx, bool debug);
    BG96Interface(PinName CELL_TX, PinName CELL_RX, PinName reset = PE14, PinName wakeup = PE8, bool debug = false);    
    virtual     ~BG96Interface();
 
    int         init(void);														
   

     
    BG96 _BG96;

};


#endif


