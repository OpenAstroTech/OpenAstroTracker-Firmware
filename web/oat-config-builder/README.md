# OAT Configurator
This is a small React-based app, using AntDesign that asks the user a bunch of questions about what components
they used to build their tracker and then spits out the defines that need to be placed in configuration_local.hpp.

# Status
This is a very early version. There are probably a few other things that we could gather from this wizard.

# Roadmap
- Pulley teeth - query for 16T vs. 20T
- Wifi settings - We should gather these here and export them to the configuration file.
- Stepper parameters - Should we support RMS Current, speed, acceleration?
- Microstep settings?
- Digital Level (Gyro) - support axis swap?
- AzAlt addon - currently not implemented.
- Bluetooth - not currently queried.
- Dark mode support?


# Getting Started 

This project was bootstrapped with [Create React App](https://github.com/facebook/create-react-app).

## Available Scripts

In the project directory, you can run:

### `npm start`

Runs the app in the development mode.\
Open [http://localhost:3000](http://localhost:3000) to view it in the browser.

The page will reload if you make edits.\
You will also see any lint errors in the console.

### `npm run build`

Builds the app for production to the `build` folder.\
It correctly bundles React in production mode and optimizes the build for the best performance.

The build is minified and the filenames include the hashes.\
Your app is ready to be deployed!

See the section about [deployment](https://facebook.github.io/create-react-app/docs/deployment) for more information.

