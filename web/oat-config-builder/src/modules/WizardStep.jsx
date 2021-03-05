import React from 'react';
import { useState } from 'react';
import {
    Button,
    Image,
    List,
    Select,
    Steps,
    Radio,
} from 'antd';

import {
    useParams
} from 'react-router-dom'

const { Step } = Steps;

const WizardStep = (props) => {
    const [stepIndex, setStepIndex] = useState(0);
    const [showResult, setShowResult] = useState(false);
    const [configuration, setConfiguration] = useState([]);


    const onRestart = () => {
        setConfiguration([]);
        setStepIndex(0);
        setShowResult(false);
    };

    const onSelect = (index, e) => {
        console.log("Control step " + index + " action. Set " + stepProps[index].variable + " to ", e);
        let newConfiguration = configuration.filter(config => config.variable !== stepProps[index].variable)
        newConfiguration = [...newConfiguration, { variable: stepProps[index].variable, value: e }]
        setConfiguration(newConfiguration);
        console.log("New config:", newConfiguration);

        if (stepIndex < stepProps.length - 1) {
            setStepIndex(stepIndex + 1);
        } else {
            setShowResult(true);
        }
    }

    const stepProps = [
        {
            title: 'Hemisphere',
            label: 'Which hemisphere do you live in:',
            variable: 'hemi',
            define: 'NORTHERN_HEMISPHERE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'N', value: 'Northern Hemisphere', image: '/images/north.png', defineValue: '1' },
                    { key: 'S', value: 'Southern Hemisphere', image: '/images/south.png', defineValue: '0' }]
            },
            //control: { type: 'combo', choices: [{key:'N', value:'Northern Hemisphere'}, {key:'S', value:'Southern Hemisphere'}] },
        },
        {
            title: 'Board',
            label: 'Which microcontroller board are you using:',
            variable: 'board',
            define: 'BOARD',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'M', value: 'ATMega 2560 (or clone)', image: '/images/mega2560.png', defineValue: 'BOARD_AVR_MEGA2560' },
                    { key: 'E', value: 'ESP32', image: '/images/esp32.png', defineValue: 'BOARD_ESP32_ESP32DEV' },
                    { key: 'K1', value: 'MKS GEN L V1.0', image: '/images/mksv10.png', defineValue: 'BOARD_AVR_MKS_GEN_L_V1' },
                    { key: 'K20', value: 'MKS GEN L V2.0', image: '/images/mksv20.png', defineValue: 'BOARD_AVR_MKS_GEN_L_V2' },
                    { key: 'K21', value: 'MKS GEN L V2.1', image: '/images/mksv21.png', defineValue: 'BOARD_AVR_MKS_GEN_L_V21' },
                ]
            },
        },
        {
            title: 'RA Stepper',
            label: 'Which stepper motor are you using for RA:',
            variable: 'ra',
            define: 'RA_STEPPER_TYPE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'B', value: '28BYJ-48', image: '/images/byj48.png', defineValue: 'STEPPER_TYPE_28BYJ48' },
                    { key: 'N', value: 'NEMA 17', image: '/images/nema17.png', defineValue: 'STEPPER_TYPE_NEMA17' },
                ]
            },
        },
        {
            title: 'RA Driver',
            label: 'Which driver board are you using to drive the RA stepper motor:',
            variable: 'radrv',
            define: 'RA_DRIVER_TYPE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'U', value: 'ULN2003', image: '/images/uln2003.png', defineValue: 'DRIVER_TYPE_ULN2003' },
                    { key: 'A', value: 'Generic A4988', image: '/images/a4988.png', defineValue: 'DRIVER_TYPE_A4988_GENERIC' },
                    { key: 'T9U', value: 'TMC2209-UART', image: '/images/tmc2209.png', defineValue: 'DRIVER_TYPE_TMC2209_UART' },
                    { key: 'T9S', value: 'TMC2209-Standalone', image: '/images/tmc2209.png', defineValue: 'DRIVER_TYPE_TMC2209_STANDALONE' },
                ]
            },
        },
        {
            title: 'DEC Stepper',
            label: 'Which stepper motor are you using for DEC:',
            variable: 'dec',
            define: 'DEC_STEPPER_TYPE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'B', value: '28BYJ-48', image: '/images/byj48.png', defineValue: 'STEPPER_TYPE_28BYJ48' },
                    { key: 'N', value: 'NEMA 17', image: '/images/nema17.png', defineValue: 'STEPPER_TYPE_NEMA17' },
                    { key: 'N', value: 'NEMA 14', image: '/images/nema14.png', defineValue: 'STEPPER_TYPE_NEMA17' },
                ]
            },
        },
        {
            title: 'DEC Driver',
            label: 'Which driver board are you using to drive the DEC stepper motor:',
            variable: 'decdrv',
            define: 'DEC_DRIVER_TYPE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'U', value: 'ULN2003', image: '/images/uln2003.png', defineValue: 'DRIVER_TYPE_ULN2003' },
                    { key: 'A', value: 'Generic A4988', image: '/images/a4988.png', defineValue: 'DRIVER_TYPE_A4988_GENERIC' },
                    { key: 'T9U', value: 'TMC2209-UART', image: '/images/tmc2209.png', defineValue: 'DRIVER_TYPE_TMC2209_UART' },
                    { key: 'T9S', value: 'TMC2209-Standalone', image: '/images/tmc2209.png', defineValue: 'DRIVER_TYPE_TMC2209_STANDALONE' },
                ]
            },
        },
        {
            title: 'Display',
            label: 'What kind of display are you using:',
            variable: 'disp',
            define: 'DISPLAY_TYPE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'N', value: 'No display', image: '/images/none.png', defineValue: 'DISPLAY_TYPE_NONE' },
                    { key: 'L', value: 'LCD Shield w/ keypad', image: '/images/lcdshield.png', defineValue: 'DISPLAY_TYPE_LCD_KEYPAD' },
                    { key: 'I08', value: 'I2C LCD Shield w/ MCP23008 controller', image: '/images/lcd23008.png', defineValue: 'DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008' },
                    { key: 'I17', value: 'I2C LCD Shield w/ MCP23017 controller', image: '/images/lcd23017.png', defineValue: 'DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017' },
                    { key: 'S13', value: 'I2C 32x128 OLED w/ joystick', image: '/images/ssd1306.png', defineValue: 'DISPLAY_TYPE_LCD_JOY_I2C_SSD1306' },
                ]
            },
        },
        {
            title: 'GPS',
            label: 'Do you have the GPS add on:',
            variable: 'gps',
            define: 'USE_GPS',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'N', value: 'No GPS', image: '/images/none.png', defineValue: '0' },
                    { key: 'Y', value: 'GPS NEO-6M', image: '/images/gpsneo6m.png', defineValue: '1' },
                ]
            },
        },
        {
            title: 'Level',
            label: 'Do you have the Digital Level add on:',
            variable: 'gyro',
            define: 'USE_GYRO_LEVEL',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'N', value: 'No Digital Level', image: '/images/none.png', defineValue: '0' },
                    { key: 'Y', value: 'MPU-6050 Gyroscope', image: '/images/levelmpu6050.png', defineValue: '1' },
                ]
            },
        },
    ];

    if (showResult) {
        let defines = [];
        configuration.map(config => {
            let property = stepProps.find(prop => prop.variable === config.variable);
            let propertyValue = property.control.choices.find(choice => choice.key === config.value);
            defines = [...defines, { variable: property.define, value: propertyValue.defineValue }];
        });
        console.log(defines);

        return <div>
            <h2>Local configuration file</h2>
            <p>Copy/paste the following into your configuration_local.hpp file</p>
            {
                defines.map(define => <p class='code'>#define {define.variable} {define.value}</p>)
            }
            <br />
            <br />
            <Button type='primary' onClick={() => onRestart()}>Restart</Button>
        </div>

    } else {
        let control = null
        switch (stepProps[stepIndex].control.type) {
            case 'combo':
                control = <Select onSelect={(e) => onSelect(stepIndex, e)}>
                    {stepProps[stepIndex].control.choices.map((ch) => <Select.Option value={ch.key}>{ch.value}</Select.Option>)}
                </Select>

                break;

            case 'radio':
                control = <Radio.Group onChange={(e) => onSelect(stepIndex, e.target.value)} buttonStyle='solid'>
                    {stepProps[stepIndex].control.choices.map((ch) => <Radio.Button value={ch.key}>{ch.value}</Radio.Button>)}
                </Radio.Group>

                break;

            case 'radioimg':
                control = <List
                    bordered
                    itemLayout='horizontal'
                    dataSource={stepProps[stepIndex].control.choices}
                    renderItem={item =>
                        <List.Item>
                            <Button value={item.value} onClick={(e) => onSelect(stepIndex, item.key)} >{item.value}</Button>
                            <Image width={200} src={item.image} />
                        </List.Item>
                    }
                />

                break;
            default:
                break;
        }
        return <div>
            <Steps current={stepIndex}>
                {stepProps.map((step) => <Step title={step.title} />)}
                <Step title='Completed' />
            </Steps>
            <h2>{stepProps[stepIndex].title}</h2>
            <p>{stepProps[stepIndex].label}</p>
            {control}
            <br />
            <Button type='primary' onClick={() => setStepIndex(stepIndex - 1)} disabled={stepIndex < 1}>Back</Button>
        </div>
    }
}

export default WizardStep;
