import React from 'react';
import { useState, useEffect } from 'react';
import {
    Button,
    Image,
    Input,
    List,
    Select,
    Steps,
    Radio,
} from 'antd';


const { Step } = Steps;

const WizardStep = (props) => {
    const [stepIndex, setStepIndex] = useState(-1);
    const [showResult, setShowResult] = useState(false);
    const [configuration, setConfiguration] = useState([]);
    const [advanceStep, setAdvanceStep] = useState(false);

    const onRestart = () => {
        setConfiguration([]);
        setStepIndex(0);
        setShowResult(false);
    };

    useEffect(() => {
        let nextStepIndex = stepIndex + 1;

        // Check whether the current step needs to be skipped
        let skip = true;
        while (nextStepIndex < stepProps.length) {
            skip = false;
            let nextStep = stepProps[nextStepIndex];
            if (nextStep.conditions) {
                // console.log("Next step has conditions:", nextStep.conditions)
                let result = true;
                nextStep.conditions.forEach(cond => {
                    const conf = configuration.find(config => config.variable === cond.variable);
                    if (!conf || (conf.value !== cond.neededKey)) {
                        result = false;
                    }
                });

                if (!result) {
                    skip = true;
                }
            }

            if (!skip) {
                setStepIndex(nextStepIndex);
                break;
            }
            else {
                nextStepIndex++;
            }
        }

        if (skip) {
            setStepIndex(nextStepIndex);
            setShowResult(true);
        }
    }, [advanceStep]);

    const onSelect = (index, e) => {
        // console.log("Control step " + index + " action. Set " + stepProps[index].variable + " to ", e);
        let newConfiguration = configuration.filter(config => config.variable !== stepProps[index].variable)
        newConfiguration = [...newConfiguration, { variable: stepProps[index].variable, value: e }]
        setConfiguration(newConfiguration);
        // console.log("New config:", newConfiguration);
        setAdvanceStep(!advanceStep);
    }

    const onChangedText = (index, key, val) => {
        // console.log("Control step " + index + " action. Set " + stepProps[index].variable + "[" + key + "] to ", val);

        if (key === '$OK') {
            setAdvanceStep(!advanceStep);
        }
        else {
            let currentConfig = configuration.find(config => config.variable === stepProps[index].variable) || { value: [] };
            let newConfig = currentConfig.value.filter(config => config.key !== key);
            newConfig = [...newConfig, { key: key, value: val }];
            let newConfiguration = configuration.filter(config => config.variable !== stepProps[index].variable);
            newConfiguration = [...newConfiguration, { variable: stepProps[index].variable, value: newConfig }]
            setConfiguration(newConfiguration);
            // console.log("New config:", newConfiguration);
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
            title: 'Use WiFi',
            label: 'Do you want to enable WiFi:',
            variable: 'wifi',
            conditions: [{ variable: 'board', neededKey: 'E' }],
            define: 'WIFI_ENABLED',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'Y', value: 'Yes, use WiFi', image: '/images/wifi.png', defineValue: '1' },
                    { key: 'N', value: 'No, disable WiFi', image: '/images/nowifi.png', defineValue: '0' },
                ]
            },
        },
        {
            title: 'WiFi Mode',
            label: 'In what mode do you want to use WiFi:',
            conditions: [{ variable: 'wifi', neededKey: 'Y' }],
            variable: 'wifimode',
            define: 'WIFI_MODE',
            control: {
                type: 'radioimg',
                choices: [
                    { key: 'I', value: 'Infrastructure, all devices connect to same network', image: '/images/infra.png', defineValue: 'WIFI_MODE_INFRASTRUCTURE' },
                    { key: 'A', value: 'Access Point, OAT is a hotspot', image: '/images/ap.png', defineValue: 'WIFI_MODE_AP_ONLY' },
                    { key: 'F', value: 'Infrastructure with failover to Access Point', image: '/images/failover.png', defineValue: 'WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP' },
                ]
            },
        },
        {
            title: 'WiFi Infrastructure Setup',
            label: 'Enter the WiFi parameters for Infrastructure mode:',
            variable: 'wifiparams',
            conditions: [
                { variable: 'wifi', neededKey: 'Y' },
                { variable: 'wifimode', neededKey: 'I' },
            ],
            define: '',
            control: {
                type: 'textinput',
                choices: [
                    { key: 'S', label: 'WiFi SSID', defineLine: '#define WIFI_INFRASTRUCTURE_MODE_SSID "{0}"' },
                    { key: 'P', label: 'WPA Key', defineLine: '#define WIFI_INFRASTRUCTURE_MODE_WPAKEY "{0}"' },
                    { key: 'H', label: 'Hostname', defaultValue: 'OATScope', defineLine: '#define WIFI_HOSTNAME "{0}"' },
                ]
            },
        },
        {
            title: 'WiFi Access Point Setup',
            label: 'Enter the WiFi parameters for Access Point mode:',
            variable: 'wifiparams',
            conditions: [
                { variable: 'wifi', neededKey: 'Y' },
                { variable: 'wifimode', neededKey: 'A' },
            ],
            define: '',
            control: {
                type: 'textinput',
                choices: [
                    { key: 'P', label: 'WPA Key for OAT hotspot', defineLine: '#define WIFI_AP_MODE_WPAKEY "{0}"' },
                    { key: 'H', label: 'Hostname', defaultValue: 'OATScope', defineLine: '#define WIFI_HOSTNAME "{0}"' },
                ]
            },
        },
        {
            title: 'WiFi Failover Setup',
            label: 'Enter the WiFi parameters for Failover mode:',
            variable: 'wifiparams',
            conditions: [
                { variable: 'wifi', neededKey: 'Y' },
                { variable: 'wifimode', neededKey: 'F' },
            ],
            define: '',
            control: {
                type: 'textinput',
                choices: [
                    { key: 'S', label: 'WiFi SSID of network', defineLine: '#define WIFI_INFRASTRUCTURE_MODE_SSID "{0}"' },
                    { key: 'P', label: 'WPA Key for network', defineLine: '#define WIFI_INFRASTRUCTURE_MODE_WPAKEY "{0}"' },
                    { key: 'N', label: 'WPA Key for OAT hotspot', defineLine: '#define WIFI_AP_MODE_WPAKEY "{0}"' },
                    { key: 'H', label: 'Hostname', defaultValue: 'OATScope', defineLine: '#define WIFI_HOSTNAME "{0}"' },
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

    if (stepIndex < 0) {
        return <div />
    }

    let steps = [];

    stepProps.forEach((step, index) => {
        let title = step.title;
        let description;
        if (index < stepIndex) {
            let foundConfig = configuration.find(config => config.variable === stepProps[index].variable);
            if (foundConfig && !Array.isArray(foundConfig.value)) {
                let foundControl = stepProps[index].control.choices.find(choice => foundConfig.value === choice.key);
                description = foundControl.value;
            }
        }

        steps.push(<Step title={title} description={description} />)
    });

    steps.push(<Step title='Completed' />)

    if (showResult) {
        let defines = [];
        configuration.forEach(config => {
            let property = stepProps.find(prop => prop.variable === config.variable);
            let defineLine = null;
            if (property.control.type === 'textinput') {
                property.control.choices.forEach(choice => {
                    let configVal = config.value.find(cfgval => cfgval.key === choice.key);
                    let val = (configVal ? configVal.value : null) || choice.defaultValue || '';
                    defineLine = choice.defineLine.replace('{0}', val);
                    defines = [...defines, defineLine];
                })
            }
            else {
                let propertyValue = property.control.choices.find(choice => choice.key === config.value);
                defines = [...defines, '#define ' + property.define + ' ' + propertyValue.defineValue];
            }
        });
        // console.log(defines);


        return <div class='steps-container'>
            <div class='steps-column'>
                <Steps current={stepIndex} direction='vertical'>
                    {steps}
                </Steps>
            </div>
            <div class='list-container'>
                <h2>Local configuration file</h2>
                <p>Copy/paste the following into your configuration_local.hpp file</p>
                {
                    defines.map(define => <p class='code'>{define}</p>)
                }
                <br />
                <br />
                <Button type='primary' onClick={() => onRestart()}>Restart</Button>
            </div>
        </div>

    } else {
        let control = null
        const stepControl = stepProps[stepIndex].control;
        switch (stepControl.type) {
            case 'combo':
                control = <Select onSelect={(e) => onSelect(stepIndex, e)}>
                    {stepControl.choices.map((ch) => <Select.Option value={ch.key}>{ch.value}</Select.Option>)}
                </Select>

                break;

            case 'radio':
                control = <Radio.Group onChange={(e) => onSelect(stepIndex, e.target.value)} buttonStyle='solid'>
                    {stepControl.choices.map((ch) => <Radio.Button value={ch.key}>{ch.value}</Radio.Button>)}
                </Radio.Group>

                break;

            case 'radioimg':
                control = <List
                    bordered
                    itemLayout='horizontal'
                    dataSource={stepControl.choices}
                    renderItem={item =>
                        <List.Item>
                            <Button value={item.value} onClick={(e) => onSelect(stepIndex, item.key)} >{item.value}</Button>
                            <Image className='image-column' src={item.image} />
                        </List.Item>
                    }
                />

                break;

            case 'textinput':
                control = <>
                    { stepControl.choices.map(input =>
                        <div style={{ marginBottom: '10pt' }}>
                            <Input addonBefore={input.label} placeholder={input.label} defaultValue={input.defaultValue} onChange={(e) => onChangedText(stepIndex, input.key, e.target.value)} />
                        </div>
                    )}
                    <div className='back-button' >
                        <Button value='OK' type='primary' onClick={(e) => onChangedText(stepIndex, '$OK')} >Next</Button>
                    </div>
                    <br></br>
                </>

                break;
            default:
                break;
        }

        return <div class='steps-container'>
            <div class='steps-column'>
                <Steps current={stepIndex} direction='vertical'>
                    {steps}
                </Steps>
            </div>
            <div class='list-container'>
                <div className='step-title'>{stepProps[stepIndex].title}</div>
                <div className='step-description'>{stepProps[stepIndex].label}</div>
                <div>
                    {control}
                </div>
                <div className='back-button' >
                    <Button type='primary' onClick={() => setStepIndex(stepIndex - 1)} disabled={stepIndex < 1}>Back</Button>
                </div>
            </div>
        </div>
    }
}

export default WizardStep;
