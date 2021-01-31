
/*
 *  SW for the Arduino-based FoodTracker, written by Jorma Kuha. 
 *  For more information, please go to http://jorkke.github.io
 *  
 *  Version history: 
 *  January, 2021: First release.

Copyright (c) 2021 Jorma Kuha

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 * 
 */
#include <MCP23S17.h>
#include <SPI.h>
/////////////////////////////////////////////////////////////////
// MCP23S17 communication (for the 30 x 5 button/keyboard matrix)

const uint8_t chipSelect = 7;  // the CS-line used - selected D7

// other pins on Uno: D11(SI), D12 (SO), D13 (SCK) - on MKR 1010 these are different


// Create an object for each chip
// Bank 0 is address 0
// Bank 1 is address 1.
// Increase the addresses by 2 for each BA value.

MCP23S17 mcp0(&SPI, chipSelect, 0);  // chip 1
MCP23S17 mcp1(&SPI, chipSelect, 1);  // chip 2
MCP23S17 mcp2(&SPI, chipSelect, 2);  // chip 3


struct McpPort
{
	MCP23S17* mcp;
	int port;
	// port has values:
	// GPB0:8
	// GPB1:9
	// ...
	// GPB7:15
	// GPA7: 7
	// GPA6: 6
	// ...
	// GPA0: 0
};

//  as a general advice, don't use GPA7 or GPB7 as inputs when using a MCP23017!
// see: https://microchipsupport.force.com/s/article/On-MCP23008-MCP23017-SDA-line-change-when-GPIO7-input-change
// however, MCP23S17 should be OK

const int Rows = 30;
const int Columns = 5;

const McpPort rowPorts[Rows] =
{
	{ &mcp0, 8},
	{ &mcp0, 9},
	{ &mcp0, 10},
	{ &mcp0, 11},
	{ &mcp0, 12},
	{ &mcp0, 13},
	{ &mcp0, 14},
	{ &mcp0, 15},

	{ &mcp0, 7},
	{ &mcp0, 6},
	{ &mcp0, 5},
	{ &mcp0, 4},
	{ &mcp0, 3},
	{ &mcp0, 2},
	{ &mcp0, 1},
	{ &mcp0, 0},

	{ &mcp1, 8},
	{ &mcp1, 9},
	{ &mcp1, 10},
	{ &mcp1, 11},
	{ &mcp1, 12},
	{ &mcp1, 13},
	{ &mcp1, 14},
	{ &mcp1, 15},

	{ &mcp1, 7},
	{ &mcp1, 6},
	{ &mcp1, 5},
	{ &mcp1, 4},
	{ &mcp1, 3},
	{ &mcp1, 2}

};

const McpPort columnPorts[Columns] =
{
	{ &mcp2, 12},
	{ &mcp2, 11},
	{ &mcp2, 10},
	{ &mcp2, 9},
	{ &mcp2, 8}
};

const int PanelButtonRows = 8;
const int PanelButtonColumns = 1;

const McpPort panelButtonRowPorts[ PanelButtonRows] =
{
	{ &mcp2, 7}, // Tare, returned as row 1
	{ &mcp2, 6}, // undo
	{ &mcp2, 5}, // ok
	{ &mcp2, 4}, // todays kcal
	{ &mcp2, 3}, // test
	{ &mcp2, 2}, // calibrate
	{ &mcp2, 1},
	{ &mcp2, 0}
};

enum panelButtonsPerRow
{
	TareButton = 1,
	UndoButton = 2,
	// ResetButton   does not go through MCP
	OKButton = 3,
	DailyKCalSoFarButton = 4,
	TestButton = 5,
	CalibrateButton = 6
};

const McpPort panelButtonColumnPorts[ PanelButtonColumns] =
{
	{ &mcp2, 15}
};

void setup_MCPs()
{
	mcp0.begin();
	mcp1.begin();
	mcp2.begin();

	// key idea:
	// mcp0 & mcp1 communicate with the rows (1-30),
	// mcp 2 communicates with the colums 1-5
	//
	// The rows are configured as INPUTs with PULLUPs high ALWAYS, which implies they are in HIGH when switch open
	// The columns are configures as OUTPUTs, being LOW when measured
	// Thus,
	// - if switch open, rows is HIGH
	// - if switch closed, row is LOW (because it is measured after the PULLUP)
	//
	// All this is managed so that in the setup, all the Column OUTPUTs are set (implicitly) HIGH.
	// Then, in a loop, one by one the Columns are set low, and all the Rows are measured for idnentifying a LOW-state => a keypress

	int column = 0;
	for (column = 0; column < Columns; column++)
	{

		columnPorts[column].mcp->pinMode(columnPorts[column].port, INPUT_PULLUP);
	}

	int row = 0;
	for (row = 0; row < Rows; row++)
	{
		rowPorts[row].mcp->pinMode(rowPorts[row].port, INPUT_PULLUP);
	}

	// panel buttons separately:

	for (column = 0; column < PanelButtonColumns; column++)
	{

		panelButtonColumnPorts[column].mcp->pinMode(panelButtonColumnPorts[column].port, INPUT_PULLUP);
	}


	for (row = 0; row < PanelButtonRows; row++)
	{
		panelButtonRowPorts[row].mcp->pinMode(panelButtonRowPorts[row].port, INPUT_PULLUP);
	}
};

bool scan_matrix(
    const McpPort* rowArray, const int rowArraySize, const McpPort* columnArray, const int columnArraySize, int& result_row, int& result_column, int asLastRow = -1, int asLastColumn = -1)
//
// retursn true if key pressed, false otherwise.
// if pressed, row contains the row 1-30, column contains the col 1-5
//
// parameter "asLastRow" is 1-30 if given, "asLastColumn" is 1-5 if given.
//
// special feature: if the two last parameters "asLastRow" and "asLastColumn" are not 
// in their default values, they indicate a button which is to be scanned as last. 
// This is due to an error detection mechanism consisting of two aspects:
// 1) we scan each button press twice, and consider it as a true keypress only if 
// both reads give the same result
// 2) on the second scan, the button that was previously scanned as pressed is scanned as last. 
//
// THis manner we also reject button presses where more than one button would indicate as pressed. 
//
// This is in order to reduce casual errors (I was getting a "ghost read" approx once a day - or even a few times in an hour).

{
	int row;
	int column;
  
    bool foundPressedButton = false;

	for (column = 0 ; column < columnArraySize; column++)
	{
        columnArray[column].mcp->pinMode(columnArray[column].port, OUTPUT);
		columnArray[column].mcp->digitalWrite(columnArray[column].port, LOW); // provide path to ground

		for (row = 0; row < rowArraySize; row++)
		{
            if ((column == (asLastColumn-1)) && (row == (asLastRow-1))) // this row/column button is to be scanned as last
            {
                continue;    
            }
			if (rowArray[row].mcp->digitalRead( rowArray[row].port) == LOW)
			{
				result_row = row + 1;
				result_column = column + 1;
                foundPressedButton = true;
				break;
			}
		}        
		columnArray[column].mcp->pinMode(columnArray[column].port, INPUT_PULLUP); // disconnect GND
        if (foundPressedButton)
        {
            break;
        }
	}
   
    // scan the last button only, if needed:
    if (!foundPressedButton && (asLastColumn != -1) && (asLastRow != -1))
    {
        column = asLastColumn-1;
        row = asLastRow-1;
        columnArray[column].mcp->pinMode(columnArray[column].port, OUTPUT);
        columnArray[column].mcp->digitalWrite(columnArray[column].port, LOW); // provide path to ground

        if (rowArray[row].mcp->digitalRead( rowArray[row].port) == LOW)
        {
            result_row = row + 1;
            result_column = column + 1;
            foundPressedButton = true;
        }
        columnArray[column].mcp->pinMode(columnArray[column].port, INPUT_PULLUP); // disconnect GND                
    }
    
    if (foundPressedButton) 
    {
        // scan for the second time, if not done yet:
        if ((asLastColumn == -1) && (asLastRow == -1)) // this was the first scan
        {
            int secondRow = 0; 
            int secondColumn = 0;
            if (!scan_matrix(rowArray, rowArraySize, columnArray, columnArraySize, secondRow, secondColumn, result_row, result_column))
            {
                Serial.println("False keypress number one!");
                return false;                
            }
            else
            {
                if ((secondRow == result_row) && (secondColumn == result_column))
                {
                    return true;
                }
                else
                {
                    Serial.println("False keypress number two!");                    
                    return false; // at least one of the reads was a ghost-read
                }
            }
        }
        else
        {
            return true;
        }
    }
    else
    {        
        return false;
    }
};


#include <HX711_ADC.h>
//////////////////////////
// loadcell initialization
//////////////////////////
//pins:
const int HX711_dout = 5; //mcu > HX711 dout pin - DAT on HX711 - this is DIGITAL pin on arduino
const int HX711_sck = 4; //mcu > HX711 sck pin- CLK on HX711 - this is DIGITAL pin on arduino

long loadcell_t; // for tracking milliseconds

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

float calibrationValue = 450.46; // calibration value (see example file "Calibration.ino")
//
// this is just a default value - at the setup, this is read from the google-sheet
// and if a value exists there, it overrides this value. 
// Likewise, if new calibration is done, the value is written to google-sheet


#include <WiFiNINA.h>
////////////////////////////////////////////////
// Communication to/from Internet/Google sheets:

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;     // the Wifi radio's status

// initialize the Wifi client library
WiFiSSLClient client;

// server address:
const char server[] = "script.google.com";
const char serverDirectory[] = SECRET_SERVER_DIRECTORY;
// the SECRET_SERVER_DIRECTORY is of the form "/macros/s/<!!! YOUR SECRET SCRIPT ID HERE !!!>/exec"

//////////////////////////////////////////////////////
String codeValueFromColumnAndRow(int column, int row)
// column from 1 onwards
// they generate a string where:
// column 1 => 'A'
// column 2 => 'B'
// ...
// column 5 => 'E'
// and
// row 1 => '1'
// row 2 => '2'
// ...
// row 30 => "30"
//
// thus for example row==1 and column==1 becomes "A1"
{
	String answer;
	switch (column)
	{
	case 1:
		answer = "A";
		break;
	case 2:
		answer = "B";
		break;
	case 3:
		answer = "C";
		break;
	case 4:
		answer = "D";
		break;
	case 5:
		answer = "E";
		break;
    case 6:
        answer = "F"; // userd to pass configuration data
        break;
      
	default:
		Serial.println("Impossible column value in codeValueFromColumnAndRow!!!");
	}
	if (row < 1 )
	{
		Serial.println("Impossible row value in codeValueFromColumnAndRow!!!");
	}
	return answer + String(row);
};

String defaultWeightsForFoodCodes;
// this is a string, for example like "A2=250;A3=250;A4=10.5;A5=1;"
// which contains default weights for selections.
// This data is maintained in the google sheet, and this string is initiated
// at the setup
//////////////////////////////////////////////////////////////////////////////
float defaultWeight(const int column, const int row)
// returns a negative value if not found
{
	int startIndex = defaultWeightsForFoodCodes.indexOf(codeValueFromColumnAndRow( column, row) + "=");
	// have to have '=' in order not to take for example code "A21" as "A2"

	if (startIndex < 0)
	{
		return -1;
	}

	// search for the next "=" from here
	startIndex = defaultWeightsForFoodCodes.indexOf("=", startIndex);

	// search for the next ";" from here
	int stopIndex = defaultWeightsForFoodCodes.indexOf(";", startIndex);

	float result = (defaultWeightsForFoodCodes.substring(startIndex + 1, stopIndex)).toFloat();
	Serial.print("\nDefault weight: ");
	Serial.println(result);
	Serial.print("Start Index: ");
	Serial.println(startIndex);
	Serial.print("Stop Index: ");
	Serial.println(stopIndex);
	return result;
};


//this function is based on http://esp32-server.de/google-sheets/?fbclid=IwAR2wGArRBaJ7YcHkVmJkEzTxiCxU443Q7cUroPHu2L7KBjntPm8m0HyjfoY
// although it had to be modified a bit in order to work with Arduino MKR 1010 wifi
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
boolean sendToGoogleSheet( const String& columnName1, const String& value1, String& result, boolean waitForReturnValue = false, const String& columnName2 = "NULL", 
const String value2 = "NULL")
//
// a special feature: we wait for a response to HTTP GET-event if "waitForReturnValue" is true, otherwise not in order to make everything faster
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

	Serial.println("Sending to google-sheet");
	client.setTimeout(15000L); // to 15 seconds. I was getting occasional connection errors on the next line, therefore trying if this helps.

	// try connecting at most 5 times
	int i = 0;
	while (true)
	{
		i++;
		if (i > 4)
		{
			Serial.println("Connecting failed!");
			return false;
		}
		if (client.connect(server, 443))
		{
			break;
		}
	}

	// make a HTTP request:

	String GETrequest;
	GETrequest += "GET ";
	GETrequest += serverDirectory;
	GETrequest += '?';
	GETrequest += columnName1;
	GETrequest += '=';
	GETrequest += value1;
    if (columnName2 != "NULL")
    {
    	GETrequest += '&';
	    GETrequest += columnName2;
	    GETrequest += '=';
	    GETrequest += value2;
    }
    GETrequest +=  " HTTP/1.1";    

	Serial.print("GET request: ");
	Serial.print(GETrequest);

	client.println(GETrequest);

	client.println("Host: script.google.com");
	client.println("Connection: close");
	client.println();

	// speeding up this function by skipping reading the forwarded URL (return value) unless specificly asked for
	if (waitForReturnValue)
	{
		String line;
		String movedURL;
		while (client.connected())
		{
			line = client.readStringUntil('\n');
			Serial.println(line);
			if (line == "\r") break;
			if (line.indexOf("Location") >= 0)
				movedURL = line.substring(line.indexOf(":") + 2);
		}

		client.stop();

		movedURL.trim();
		Serial.println("Forwarding URL: \"" + movedURL + "\"");
		if (movedURL.length() < 10) return false;

		Serial.println("Start forwarding...");
		// try at most 5 times:
		int i = 0;
		while (true)
		{
			i++;
			if (i > 4)
			{
				Serial.println("Forwarding failed!");
				return false;
			}
			if (client.connect(server, 443))
			{
				break;
			}
		}
		client.println("GET " + movedURL);
		client.println("Host: script.google.com");
		client.println("Connection: close");
		client.println();

		while (client.connected()) //  receive headers
		{
			line = client.readStringUntil('\n');
			Serial.println(line);
			if (line == "\r") break;
		}
		while (client.connected() && client.available()) // Google Answer HTML Read line by line
		{
			line = client.readStringUntil('\r'); // The value in the cell will be here
			Serial.println(line);
			result = line;
			Serial.println("");
			Serial.print("result: ");
			Serial.println(result);
			Serial.println("");
		}
	}
	client.stop();
	return true;
};

boolean deleteLatestDataRowFromGoogleSheet( String& result)
{
	const String s = "DeleteFirstRow";

	return sendToGoogleSheet( s, s, result); // don't wait for return value. 
};

boolean getCodesWithServingsDefined(String& codesWithServings)
// returns the codes in a ";" -separated list which can be used without measuring the weight (because a default serving size has been defined)
{
	const String s = "GetCodesWithServings";

	return sendToGoogleSheet(s, s, codesWithServings, true);
};

boolean storeCalibrationValue(float calibrationValue)
// returns the codes in a ";" -separated list which can be used without measuring the weight (because a default serving size has been defined)
{
	String s = "StoreCalibrationValue";
    String parameter(calibrationValue);        

	return sendToGoogleSheet(s,  parameter, s);  
};

boolean getTodaysKcal(String& todaysKcal)
// returns the codes in a ";" -separated list which can be used without measuring the weight (because a default serving size has been defined)
// also returns possible configuration values defined in the same google-sheet
{
	const String s = "GetTodaysKcal";

	return sendToGoogleSheet(s, s, todaysKcal, true);  
};

#include "LedControl.h"
/////////////////////////////
// led matrix initialization
/////////////////////////////

LedControl lc = LedControl(2, 1, 0, 4);
// Pins: DIN,CLK,CS, # of Display connected. No need to use the actual parallel interface.
// Note: the pins refer to digital pins.

// Put char-values into arrays:

struct CharData
{
	byte b[8];
};

CharData zeroKgText[4] =
{
	{	// '0'
		B00001110,
		B00010001,
		B00010001,
		B00010001,
		B00010001,
		B00010001,
		B00010001,
		B00001110
	},
	{	// 'K'
		B00000000,
		B01111011,
		B00110011,
		B00110110,
		B00111100,
		B00110110,
		B00110110,
		B01111011
	},
	{	// 'G'
		B00000000,
		B00011110,
		B00110011,
		B00100000,
		B00100000,
		B00100111,
		B00110011,
		B00011110
	},
	{	// '?'
		B00000000,
		B00011110,
		B00110011,
		B00000011,
		B00000110,
		B00001100,
		B00000000,
		B00001100
	}
};

CharData oneKgText[4] =
{
	{	//'1'
		B00000010,
		B00000110,
		B00000010,
		B00000010,
		B00000010,
		B00000010,
		B00000010,
		B00000111
	},
	{	// 'K'
		B00000000,
		B01111011,
		B00110011,
		B00110110,
		B00111100,
		B00110110,
		B00110110,
		B01111011
	},
	{	// 'G'
		B00000000,
		B00011110,
		B00110011,
		B00100000,
		B00100000,
		B00100111,
		B00110011,
		B00011110
	},
	{	// '?'
		B00000000,
		B00011110,
		B00110011,
		B00000011,
		B00000110,
		B00001100,
		B00000000,
		B00001100
	}
};



CharData waitText[4] =
{
	{	// 'W'
		B00000000,
		B00000000,
		B00000000,
		B01000001,
		B01001001,
		B00101010,
		B00101010,
		B00010100
	},
	{	// 'A'
		B00000000,
		B00000000,
		B00001100,
		B00010010,
		B00010010,
		B00011110,
		B00010010,
		B00010010
	},
	{	// 'I'
		B00000000,
		B00000000,
		B00001110,
		B00000100,
		B00000100,
		B00000100,
		B00000100,
		B00001110
	},
	{	// 'T'
		B00000000,
		B00000000,
		B00011111,
		B00000100,
		B00000100,
		B00000100,
		B00000100,
		B00000100
	}
};


CharData failText[4] =
{
	{	// 'F'
		B00000000,
		B00000000,
		B00001111,
		B00001000,
		B00001110,
		B00001000,
		B00001000,
		B00001000
	},

	{	// 'A'
		B00000000,
		B00000000,
		B00001100,
		B00010010,
		B00010010,
		B00011110,
		B00010010,
		B00010010
	},
	{	// 'I'
		B00000000,
		B00000000,
		B00001110,
		B00000100,
		B00000100,
		B00000100,
		B00000100,
		B00001110
	},

	{	// 'L'
		B00000000,
		B00000000,
		B00001000,
		B00001000,
		B00001000,
		B00001000,
		B00001000,
		B00001111
	}
};


CharData spaceChar =
{
	B00000000,  //Space (Char 0x20)
	B00000000,
	B00000000,
	B00000000,
	B00000000,
	B00000000,
	B00000000,
	B00000000
};

CharData positiveNumbers[10] =
{
	{
		B00001110, //0
		B00010001,
		B00010001,
		B00010001,
		B00010001,
		B00010001,
		B00010001,
		B00001110
	},

	{
		B00000010,  //1
		B00000110,
		B00000010,
		B00000010,
		B00000010,
		B00000010,
		B00000010,
		B00000111
	},

	{
		B00001110,  //2
		B00010001,
		B00000001,
		B00000010,
		B00000100,
		B00001000,
		B00010000,
		B00011111
	},

	{
		B00011111,  //3
		B00000001,
		B00000010,
		B00000100,
		B00000010,
		B00100001,
		B00110001,
		B00011110
	},

	{
		B00000000,  //4
		B00001001,
		B00001001,
		B00001001,
		B00001111,
		B00000001,
		B00000001,
		B00000001
	},

	{
		B00011111,  //5
		B00010000,
		B00010000,
		B00011110,
		B00000001,
		B00000001,
		B00010001,
		B00001110
	},

	{
		B00001110,  //6
		B00010000,
		B00010000,
		B00010000,
		B00011110,
		B00010001,
		B00010001,
		B00001110
	},

	{
		B00011111,  //7
		B00010001,
		B00000001,
		B00000010,
		B00000100,
		B00000100,
		B00000100,
		B00000100
	},

	{
		B00001110,  //8
		B00010001,
		B00010001,
		B00001110,
		B00010001,
		B00010001,
		B00010001,
		B00001110
	},

	{
		B00001110,  //9
		B00010001,
		B00010001,
		B00001111,
		B00000001,
		B00000001,
		B00000001,
		B00001110
	}
};

CharData negativeNumbers[10] =
{
	{
		B00001110, //0
		B00010001,
		B00010001,
		B11010001,
		B00010001,
		B00010001,
		B00010001,
		B00001110
	},

	{
		B00000010,  //1
		B00000110,
		B00000010,
		B11000010,
		B00000010,
		B00000010,
		B00000010,
		B00000111
	},

	{
		B00001110,  //2
		B00010001,
		B00000001,
		B11000010,
		B00000100,
		B00001000,
		B00010000,
		B00011111
	},

	{
		B00011111,  //3
		B00000001,
		B00000010,
		B11000100,
		B00000010,
		B00100001,
		B00110001,
		B00011110
	},

	{
		B00000000,  //4
		B00001001,
		B00001001,
		B11001001,
		B00001111,
		B00000001,
		B00000001,
		B00000001
	},

	{
		B00011111,  //5
		B00010000,
		B00010000,
		B11011110,
		B00000001,
		B00000001,
		B00010001,
		B00001110
	},

	{
		B00001110,  //6
		B00010000,
		B00010000,
		B11010000,
		B00011110,
		B00010001,
		B00010001,
		B00001110
	},

	{
		B00011111,  //7
		B00010001,
		B00000001,
		B11000010,
		B00000100,
		B00000100,
		B00000100,
		B00000100
	},

	{
		B00001110,  //8
		B00010001,
		B00010001,
		B11001110,
		B00010001,
		B00010001,
		B00010001,
		B00001110
	},

	{
		B00001110,  //9
		B00010001,
		B00010001,
		B11001111,
		B00000001,
		B00000001,
		B00000001,
		B00001110
	}
};


// Display number
void displaySingleNumber(
    int display, // display, from 0 to 3
    int singleNumber, // number to display, from -9 to 9
    int firstNumberShown = false)// if true, show abs (no sign)
{
	for (int i = 0; i < 8; i++)
	{
		if (singleNumber >= 0 || firstNumberShown)
		{
			lc.setRow(display, i, positiveNumbers[abs(singleNumber)].b[i]);
		}
		else
		{
			lc.setRow(display, i, negativeNumbers[abs(singleNumber)].b[i]);
		}
	}
};


class VisualizeRowColAsDots
// this class is used when testing the row/col buttons. Each button press
// is shown on LED-display as a dot in a 30*5 dot matrix
{
private:
	CharData data[4];
	VisualizeRowColAsDots() {}; // forbid constructing without parameters
	void showData()
	{
		for (int display = 0; display < 4 ; display++)
		{
			for (int i = 0; i < 8; i++)
			{
				lc.setRow(display, i, data[display].b[i]);
			}
		}
	};
public:
	VisualizeRowColAsDots( bool startBright = true)
	{
		char fillByte;
		if (startBright)
		{
			fillByte = 0xff;
		}
		else
		{
			fillByte = 0;
		};
		for (int display = 0; display < 4 ; display++)
		{
			for (int i = 0; i < 5; i++)
			{
				data[display].b[i] = fillByte;
			}
			for (int i = 5; i < 8; i++)
			{
				data[display].b[i] = ~fillByte;
			}
		}
		for (int i = 0; i < 5; i++)
		{
			data[3].b[i] ^= (1UL << 7);
			data[3].b[i] ^= (1UL << 6);
		}
		showData();
	};

	void toggle(int column, int row, bool turnOn = false)
	// column: 1-5, row: 1-30
	{
		if (column > 5 || column < 1 || row > 30 || row < 1)
		{
			Serial.println("Invalid column/row value in toggle!");
		};

		// first determine the display, 0-3
		int display = (row - 1) / 8;
		// then determine the dot
		int dot = (row - 1) % 8;

		// then toggle the dot
		if (turnOn)
		{
			data[display].b[column - 1] |= (1UL << dot); // set the bit
		}
		else
		{
			data[display].b[column - 1] &= ~(1UL << dot); // clear the bit
		}
		showData();
	}
};

void displaySpace(
    int display) // display, from 0 to 3

{
	for (int i = 0; i < 8; i++)
	{
		lc.setRow(display, i, spaceChar.b[i]);
	}
};

void displayWeight( int weight)
//  -10 000 > weight > 10 000
{

	bool firstNumberShown = false;
	if (weight > 999 || weight < -999)
	{
		displaySingleNumber(3, weight / 1000, false);
		firstNumberShown = true;
		weight %= 1000;
	}
	else
	{
		displaySpace(3);
	};


	if ((weight > 99) || (weight < -99) || firstNumberShown)
	{
		displaySingleNumber(2, weight / 100, firstNumberShown);
		firstNumberShown = true;
		weight %= 100;
	}
	else
	{
		displaySpace(2);
	};

	if ((weight > 9) || (weight < -9) || firstNumberShown )
	{
		displaySingleNumber(1, weight / 10, firstNumberShown);
		firstNumberShown = true;
		weight %= 10;
	}
	else
	{
		displaySpace(1);
	};

	displaySingleNumber( 0, weight, firstNumberShown);
};

void displayCharData( CharData* text);
// need to have this declaration due to an apparent compiler/preprocecssor bug - odes not compile without

void displayCharData( CharData* text)
{
	for (int display = 0; display < 4 ; display++)
	{
		for (int i = 0; i < 8; i++)
		{
			lc.setRow(3 - display, i, text[display].b[i]);
		}
	}
};


void calibrate() {
    Serial.println("Start calibration:");
    Serial.println("Place the load cell an a level stable surface.");
    Serial.println("Remove any load applied to the load cell.");

    displayCharData( zeroKgText);        
    int row, column;  
    while(true)
    {  
        if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
        {
            if (row == OKButton)
            {
                Serial.println("OK-button pressed to start Calibration!"); 
                break;
            }
        }
    }
    // wait until the user does not press the OK-button anymore
    while( scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
    {};
    
    Serial.println("Moving on..."); 
    displayCharData( waitText);
    LoadCell.tare(); // blocking call to Tare

    Serial.println("Now, place a 1kg mass on the loadcell.");
    displayCharData( oneKgText);        
    while (true)
    {
        if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
        {
            if (row == OKButton)
            {
                Serial.println("OK-button pressed to confirm 1kg mass on the scale!");                            
                break;
            }
        }
    }
    // wait until the user does not press the OK-button anymore
    while( scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
    {};
    
    displayCharData( waitText);    
    float known_mass = 1000.0;
    LoadCell.update();

    LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
    float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value

    Serial.print("New calibration value has been set to: ");
    Serial.print(newCalibrationValue);
    Serial.println(", use this as calibration value (calFactor) in your project sketch.");
    calibrationValue = newCalibrationValue;
    
    
   
    if (!storeCalibrationValue( calibrationValue)) // write to google sheet
    {
        // something failed when communicating with google
        Serial.println("Storing calibration value failed!");                                    
        displayCharData( failText);
        // wait here until pressing the OK button:
        while (true)
        {
            if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
            {
                if (row == OKButton)
                {
                    break;
                }
            }
        }
    }
        
    Serial.println("End calibration");
};

int dailyKcalTimeoutInMinutes = 15; 
// this is the timeout in minutes that the screen starts to show the daily kcal so far automatically, if no activity. 
int dailyKcalRefreshInMinutes = 30; 
// if the daily kcal expired, how oftem is the kcal-display refreshed by re-reading the value from google sheet

unsigned long timeOfTheLastButtonPress= 0;
// time since the previous button press

void setup() {
	// put your setup code here, to run once:

	lc.shutdown(0, false); // Wake up displays
	lc.shutdown(1, false);
	lc.shutdown(2, false);
	lc.shutdown(3, false);

	lc.setIntensity(0, 1); // Set intensity levels (1-15)
	lc.setIntensity(1, 1);
	lc.setIntensity(2, 1);
	lc.setIntensity(3, 1);

	lc.clearDisplay(0);  // Clear Displays
	lc.clearDisplay(1);
	lc.clearDisplay(2);
	lc.clearDisplay(3);

	displayCharData( waitText);
	Serial.begin(9600);
	setup_MCPs();
	Serial.println("Started:  ");

	///////////////////
	// Loadcell setup:

	LoadCell.begin();


	long stabilizingtime = 2000; // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
	boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
	LoadCell.start(stabilizingtime, _tare);
	if (LoadCell.getTareTimeoutFlag()) {
		Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
		displayCharData( failText);
		while (1);
	}
	else {
		LoadCell.setCalFactor(calibrationValue); // set calibration value (float)
		Serial.print("Calibration value: ");
		Serial.println(calibrationValue);
		Serial.println("Loadcell Startup is complete");
	}

	////////////////////
	// Internet setup:

	// check for the WiFi module:
	if (WiFi.status() == WL_NO_MODULE) {
		Serial.println("Communication with WiFi module failed!");
		displayCharData( failText);
		// don't continue
		while (true);
	}

	String fv = WiFi.firmwareVersion();
	if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
		Serial.println("Please upgrade the firmware");
	}

	// attempt to connect to Wifi network:
	while (status != WL_CONNECTED) {
		Serial.print("Attempting to connect to WPA SSID: ");
		Serial.print(ssid);
		// Connect to WPA/WPA2 network:
		status = WiFi.begin(ssid, pass);

		// wait 10 seconds for connection:
		delay(10000);
	}

	// you're connected now, so print out the data:
	Serial.print("You're connected to the network");
	printCurrentNet();
	printWifiData();

	Serial.println("Getting codes with servings:");
	if (!getCodesWithServingsDefined( defaultWeightsForFoodCodes))
	{
		// something failed when sending to google
        Serial.println("Getting codes with servings from google failed!");                 
		displayCharData( failText);
		// wait here until pressing the OK button:
		while (true)
		{
			int row = 0;
			int column = 0;
			if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
			{
				if (row == OKButton)
				{
					break;
				}
			}
		}
	}
	Serial.println("Default weights for food codes: ");
	Serial.println(defaultWeightsForFoodCodes);

    // read the calibration code
    float newCalibrationValue = defaultWeight(6, 1+1); // F2
    if (newCalibrationValue <= 0)
    {
        Serial.println("Impossible situation - no default calib value in google-sheet!!");
    }    
    else
    {
        Serial.print("Read calibration value:");
        Serial.println(newCalibrationValue);
        calibrationValue = newCalibrationValue;
    };

    /////////////////////////////////////////////////
    // read the dailyKcalTimeout
    float newDailyKcalTimeout= defaultWeight(6, 1+2); // F3
    if (newDailyKcalTimeout <= 0)
    {
        Serial.println("Either no default dailyKcla-value in google-sheet, or it is negative!");
        dailyKcalTimeoutInMinutes = -1; // no timeout
    }    
    else
    {
        dailyKcalTimeoutInMinutes = newDailyKcalTimeout;        
        Serial.print("Read dailyKcalTimeout - value:");
        Serial.println(dailyKcalTimeoutInMinutes);
    }   

    //////////////////////////////////////////////////////
    // read the dailyKcalRefresh
    float newDailyKcalRefresh= defaultWeight(6, 1+3); // F4
    if (newDailyKcalRefresh <= 0)
    {
        Serial.println("Either no default dailyRefresh-value in google-sheet, or it is negative!");
        dailyKcalRefreshInMinutes = -1; // no timeout
    }    
    else
    {
        dailyKcalRefreshInMinutes = newDailyKcalRefresh;        
        Serial.print("Read dailyKcalRefresh - value:");
        Serial.println(dailyKcalRefreshInMinutes);
    } 
    
};

void showAndRefreshDailyKcal()
// shows daily kcal so far, and keeps refreshing it with the defined interval
{
    // exceptionally using goto, because otherwise we'd need to break out from two nested loops
    String resultStringFromGoogle;
    LOOP: 
    timeOfTheLastButtonPress = millis();
    Serial.println("Getting daily kcals so far because timeout expired");
    displayCharData(waitText);
    int row = 0; int column = 0;    
    if (getTodaysKcal(resultStringFromGoogle))
    {
        Serial.println("Return string: ");
        Serial.println(resultStringFromGoogle);
        displayWeight( resultStringFromGoogle.toFloat()); // shows todays kcals actually
        // wait here until pressing some panel button:
        while (true)
        {
            if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
            // can't use the rowports/columns - there could be a negative weight due to tare, 
            // thus default weight would not get used
            {
                timeOfTheLastButtonPress = millis();
                if (row != DailyKCalSoFarButton)
                    break;                        
            }
            if (millis() > timeOfTheLastButtonPress + ((unsigned long)dailyKcalRefreshInMinutes)*1000UL*60UL)
            // note usage of refresh here instead of timeout
            {
                goto LOOP;
            }                
        } // while
    }
    else
    {
        // something failed when communicating with google
        Serial.println("Communicating with google failed when trying to fetch daily kcal so far (timeout-fetch)!");                         
        displayCharData( failText);
        // wait here until pressing the OK button:
        while (true)
        {
            if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
            {
                timeOfTheLastButtonPress = millis();                    
                if (row == OKButton)
                {
                    break;
                }
            }
        }
    }      
};


void loop() {

	static float weight = 0;
	if (LoadCell.update()) // new data ready
	{
		weight = LoadCell.getData();
		displayWeight(round(weight));
		//Serial.print("Load_cell output val: ");
		//Serial.println(i);
	}


	int row = 0;
	int column = 0;
	String resultStringFromGoogle;


    // before scanning the keyboards, let's check if the timeout has expired for showing the daily kcal (having been idle long enough)

    if ((dailyKcalTimeoutInMinutes > 0) && (millis() > timeOfTheLastButtonPress + ((unsigned long)dailyKcalTimeoutInMinutes)*1000UL*60UL))
    {
        // yes, timeout has expired, so show the daily kcal
        // keep re-freshing this in the interval given
        showAndRefreshDailyKcal();
    }  
	else if (scan_matrix(rowPorts, Rows, columnPorts, Columns, row, column))
	{
        timeOfTheLastButtonPress = millis();
		Serial.print("Key pressed: Row ");
		Serial.print(row);
		Serial.print(", Column: ");
		Serial.println(column);

		// do some error checks:
		float weightToSend = weight;
		if ((weightToSend < 0.5)  && (weightToSend > -0.5) && ( defaultWeight(column, row + 1) < 0)  )
		{
			Serial.println("Cannot send zero weight! No default value defined!");
			displayCharData( failText);
			// wait here until pressing the OK button:
			while (true)
			{
				if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
				{
					if (row == OKButton)
					{
                        timeOfTheLastButtonPress = millis();                    
						break;
					}
				}
			}
		}
		else
		{
			if ((weightToSend < 0.5) && (weightToSend > -0.5))
			{
				// now that we are here, we know that default value is defined
				weightToSend = defaultWeight(column, row + 1);
				if (weightToSend < 0)
				{
					Serial.println("Impossible situation in main loop with weightToSend! No default weight!");
				}
			}

			Serial.println("Sending new row:");
			displayCharData(waitText);
            String gramsToSend( round(abs(weightToSend)));
			if (sendToGoogleSheet("Code", codeValueFromColumnAndRow(column, row + 1), resultStringFromGoogle, false,  "Amount_g", gramsToSend))
			{
				Serial.println("ResultStringFromGoogle: ");
				Serial.println(resultStringFromGoogle);
			}
			else
			{
				// something failed when sending to google
                Serial.println("Sending data to google failed!");                         
				displayCharData( failText);
				// wait here until pressing the OK button:
				while (true)
				{
					if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
					{
						if (row == OKButton)
						{
                            timeOfTheLastButtonPress = millis();
							Serial.println("OK-button pressed in error situation!");
							break;
						}
					}
				}
			}
		}
	}
	else if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
	{
        timeOfTheLastButtonPress = millis();
		if (row == TareButton)
		{
			Serial.println("Tare pressed!");
			displayCharData( waitText);
			LoadCell.tare(); // blocking call to Tare
			Serial.println("Tare ready!");

		}
		else if (row == UndoButton)
		{
			Serial.println("Deleting latest data");
			displayCharData( waitText);
			if (deleteLatestDataRowFromGoogleSheet(resultStringFromGoogle))
			{
				Serial.println("Return string: ");
				Serial.println(resultStringFromGoogle);
			}
			else
			{
				// something failed when communicating with google
                Serial.println("Communicating with google failed when trying to delete latest row!");                         
				displayCharData( failText);
				// wait here until pressing the OK button:
				while (true)
				{
					if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
					{
						if (row == OKButton)
						{
                            timeOfTheLastButtonPress = millis();
						    Serial.println("OK-button pressed in error situation!");
						break;
						}
					}
				}
			}
		}
		else if (row == TestButton)
		{
			VisualizeRowColAsDots tester(true);
			while (true)
			{
				int row = 0;
				int column = 0;
				if (scan_matrix(rowPorts, Rows, columnPorts, Columns, row, column))
				{
					tester.toggle(column, row, false);
				}
				else if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
				{
					if (row == OKButton)
					{
                        timeOfTheLastButtonPress = millis();                    
					    Serial.println("OK-button pressed while testing!");
					break;
					}
				}
			}
		}
		else if (row == DailyKCalSoFarButton)
		{
            showAndRefreshDailyKcal();        
		}
        else if (row == CalibrateButton)
        {
            Serial.println("Calibrate Button pressed!");
            calibrate();
            Serial.print("New calibration value:");
            Serial.println(calibrationValue);
            displayWeight(calibrationValue);
            // wait here until pressing some panel button:
            while (true)
            {
                if (scan_matrix(panelButtonRowPorts, PanelButtonRows, panelButtonColumnPorts, PanelButtonColumns, row, column))
                // can't use the rowports/columns - there could be a negative weight due to tare, 
                // thus default weight would not get used
                {
                    timeOfTheLastButtonPress = millis();                    
                    if (row != CalibrateButton)
                        break;
                }
            }            
        }

		else if (row == OKButton)
		{
			// no action
            timeOfTheLastButtonPress = millis();
		}

		else
			Serial.println("Unknown panel button pressed!");
	}
};

void printWifiData() {
	// print your board's IP address:
	IPAddress ip = WiFi.localIP();
	Serial.print("IP Address: ");
	Serial.println(ip);
	Serial.println(ip);

	// print your MAC address:
	byte mac[6];
	WiFi.macAddress(mac);
	Serial.print("MAC address: ");
	printMacAddress(mac);
}

void printCurrentNet() {
	// print the SSID of the network you're attached to:
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());

	// print the MAC address of the router you're attached to:
	byte bssid[6];
	WiFi.BSSID(bssid);
	Serial.print("BSSID: ");
	printMacAddress(bssid);

	// print the received signal strength:
	long rssi = WiFi.RSSI();
	Serial.print("signal strength (RSSI):");
	Serial.println(rssi);

	// print the encryption type:
	byte encryption = WiFi.encryptionType();
	Serial.print("Encryption Type:");
	Serial.println(encryption, HEX);
	Serial.println();
}

void printMacAddress(byte mac[]) {
	for (int i = 5; i >= 0; i--) {
		if (mac[i] < 16) {
			Serial.print("0");
		}
		Serial.print(mac[i], HEX);
		if (i > 0) {
			Serial.print(":");
		}
	}
	Serial.println();
}
