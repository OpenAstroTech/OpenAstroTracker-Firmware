import React from "react";
import {
  BrowserRouter as Router,
  Route,
  Switch
} from "react-router-dom";

import Home from './modules/Home'
import WizardStep from './modules/WizardStep'

import logo from './logo.svg';
import './App.css';
import './index.css';

function App() {
  return (

    <Router>
      <Switch>
        <Route path="/steps">
          <WizardStep />
        </Route>
        <Route path="/">
          <Home />
        </Route>
      </Switch>
    </Router>
  );
}

export default App;
