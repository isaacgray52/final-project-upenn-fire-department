[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/2TmiRqwI)
# final-project-skeleton

    * Team Name: UPenn Fire Department
    * Team Members: Isaac Gray, Oliver Hendrych
    * Github Repository URL: https://github.com/ese3500/final-project-upenn-fire-department
    * Github Pages Website URL: [for final submission]
    * Description of hardware: See '9. Components' below 

## Final Project Proposal

### 1. Abstract

In a few sentences, describe your final project. This abstract will be used as the description in the evaluation survey forms.

The UPenn Fire Department Team is building a semi-autonomous remote controlled firetruck. Using wireless communication to control the truck itself and thermal imaging to control the water gun, the truck will be able to go into areas not normally accessible by humans to put out fires. The thermal imagining will allow the truck to detect the direction and location of the fire, removing the need for direct human oversight of the relative positions of the fire truck and the fire.

### 2. Motivation

What is the problem that you are trying to solve? Why is this project interesting? What is the intended purpose?

The UPenn Firetruck will put an end to fires that are normally inaccessible to humans, and/or supervise over areas to detect fires. This project incorporates multiple different sensors and actuators, including sensors for heat and distance, as well as actuators for drive, steering, aiming, and blasting water, meaning that it is a culmination of many of the different, interesting means of control we have learnt through the semester, along with some more. Its intended purpose is to find heat sources (fires) and cool them down (with water).

### 3. Goals

These are to help guide and direct your progress.

The UPenn Firetruck will be constructed such that it can act in two modes: Manual and Overwatch. Manual will involve a user directly controlling the truck movement and possible the water gun, and will allow human operators to put out fires from a distance, and in places usually unaccessible to human firefights. Overwatch will be an always-on mode that uses the thermal imaging to scan for fires or other high-heat sources.

### 4. Software Requirements Specification (SRS)

Formulate key software requirements here.

#### Overview

The project will mainly revolve around taking in sensor input to determine motor controlled output, and using that feedback loop to operate. Wireless communication and the ultrasonic sensor will inform movement, which will cause the readings from the ultrasonic sensor and thermal camera to change. The thermal imaging will inform the water turret aiming and firing, which will in turn change the thermals seen by the camera.

#### Users

The users are anybody who needs to be able to put out fires in cramped spaces and/or autonomously. This could be people with complex cooling/heating systems, with cramped access ducts. Or even farmers overseeing grain stockpiles in hot, dry summers, who want to be able to put out fires without flooding their whole crop with water and causing it all to mold. And of course, the UPenn Firetruck is another tool for regular firefighters to add to their arsenal that can be applied in a situation where debris has caused areas to become inaccessible to humans.

#### Definitions, Abbreviations

* MT = Motors used to drive
* MC = Microcontroller
* RC = Remote Communication
* TI = Thermal Imaging
* US = Ultrasonic sensor
* WT = Water turret (and the motors used to drive it)
* WG = Water gun (and the motor used to fire it)
* WP = Waterproofing (shell for other means)

#### Functionaility

SRS 01 - 'The US shall be sampled as often as possible, and results smoothed/averaged to reduce the effect of noise'

SRS 02 - 'MT shall be controlled using PWM outputs from MC to determine speed'

SRS 03 - 'The hottest heat source in the TI shall be indentified and targeted by WT'

SRS 04 - 'If a sufficiently hot heat source is identifed by `SRS 03`, WG shall be fired'

SRS 05 - 'Output from US as specified in `SRS 01` shall prevent MT from driving device into the wall'

SRS 06 - 'Along with US input from `SRS 05` and `SRS 01`, RC shall inform output to MT using tank controls'

SRS 07 - 'Manual control of WT and WG should be possible through use of RC'

### 5. Hardware Requirements Specification (HRS)

Formulate key hardware requirements here.

#### Overview

The hardware shall be broken down into two parts: mechanical and electrical. The main goal of the mechanical section is to provide avenues to sense and control using the electrical. This includes mounting of MT, US and TI, as well as a mechanism to control WT and WG. Finally, the mechnical section should provide at least some level of WP. The electrical section will define the elements used to control each of these actuators, and the sensors that inform the MC.

#### Definitions, Abbreviations

* MT = Motors used to drive
* MC = Microcontroller
* RC = Remote Communication
* TI = Thermal Imaging
* US = Ultrasonic sensor
* WT = Water turret (and the motors used to drive it)
* WG = Water gun (and the motor used to fire it)
* WP = Waterproofing (shell for other means)

#### Functionaility

HRS 01 - 'MT shall be controlled through driving circuit, which is in turn shall be driven by MC'

HRS 02 - 'RC shall be provided by Feather ESP32 or other means, connected to MC'

HRS 03 - 'TI shall be provided by camera, which should be cheap and low resolution'

HRS 04 - 'TI shall be connected to MC, and communicate through SPI'

HRS 05 - 'TI shall be mounted to front of device, or should be mounted to WT'

HRS 06 - 'US shall be mounted to front of device, or optionally to mounted to WT'

HRS 07 - 'WT shall provide method of yaw rotation of WG'

HRS 08 - 'WT should provide method of pitch rotation of WG, or WG should be controllable in distace'

HRS 09 - 'WP shall provide protection of MT, MC, RC, TI, and US from liquids shot by WG'

### 6. MVP Demo

What do you expect to accomplish by the first milestone?

By the first milestone, we plan to have a working car that is simply able to drive around. Separately, we plan to have a working water turret or at least a mechanical mock-up of the water gun and a method to aim and actuate it, as well as a means of communicating between the thermal imaging and the microcontroller. Finally, we will have decided on and devised a method for wireless communication with the firetruck, whether that be using Blynk and the Feather ESP32, radio waves, or other means.

### 7. Final Demo

What do you expect to achieve by the final demonstration or after milestone 1?

By the final demonstration, we will have combined all the separate elements that we prepared in milestone one. This includes attaching the water gun to the firetruck and controlling it with the microcontroller (and thermal imaging), and linking the wireless communication to the microcontroller. Additionally, to protect the sensitive electronics on the firetruck, we will have some shell / outer design for the firetruck or other waterproofing.

### 8. Methodology

What is your approach to the problem?

Our approach will be at first split across the different aspects of our project. That is, the microcontroller, the thermal imaging, the water gun, the car control, power management, and the wireless controller. This will allow us to work on different parts in parallel in order to increase efficiency. Importantly, as we begin designing these separate processes, we will defines the interface by which each one interacts with the others. This is to make the next step, integration, much easier and faster. We will combine all the different aspects of the project and use the microcontroller to bring them all together.

Putting out fires that one cannot see directly is a difficult task. Evidently, some other device able to be dispatched to put our these fires is needed (i.e. the UPenn Firetruck). However, this does not solve the problem of the lack of direct visual confirmation. Video streaming is costly and complex. The more that can be done on the firetruck, the better. As such, thermal imaging is necessary to determine the source of the fire and put it out. Extra care and sensors are needed to make sure the firetruck does not get stuck or break itself by ramming into a wall.

### 9. Components

What major components do you need and why?

The major components we will need are:

* The microcontroller: to bring all the aspects of the project together give a platform for computation on the firetruck
* A thermal camera: can be cheap and low resolution, but necessary so that the firetruck can determine where the fire is to aim the water gun
* Motors and servos: To be able to drive the car, actuate the water turret, and fire the water gun
* Ultrasonic sensor: To prevent the car from breaking itself by ramming into a wall when the user cannot see it
* Wireless communication module: To allow remote control of the firetruck
* Shell or other waterproofing: Both for visual effect and to protect the sensitive electronics on the firetruck

### 10. Evaluation

What is your metric for evaluating how well your product/solution solves the problem? Think critically on this section. Having a boolean metric such as “it works” is not very useful. This is akin to making a speaker and if it emits sound, albeit however terrible and ear wrenching, declare this a success.
It is recommended that your project be something that you can take pride in. Oftentimes in interviews, you will be asked to talk about projects you have worked on.

The measures of success we will have for this problem are twofold: movement and sensing. The wireless communication and actuators on the car will work towards the movement: being able to control the car remotely is critical to overall performance, as is it being able to drive. Aiming the water turret is critical as well, but could possibly be sidestepped with driving to aim. The thermal imaging and ultrasonic sensor will help with sensing: being able to determine where the fire is with relative accuracy, and preventing the firetruck from ramming itself into a wall would be nice as well, but can also be sidestepped with rugged design. Waterproofing is an auxiliary goal, but would be nice to prevent single-use electronics.

### 11. Timeline

This section is to help guide your progress over the next few weeks. Feel free to adjust and edit the table below to something that would be useful to you. Really think about what you want to accomplish by the first milestone.

| **Week**            | **Task** | **Assigned To**    |
|----------           |--------- |------------------- |
| Week 1: 3/24 - 3/31 |  Mechanical Designs and hardware/software layout  |   Isaac and Oliver, respectively   |
| Week 2: 4/1 - 4/7   |  Fabrication and Motor control started; sensor and wireless communication basics  |     Isaac and Oliver, respectively           |
| Week 3: 4/8 - 4/14  |  Fabrication finished and motor control finalized; overarching control (sensing and actuating) logic finished    |    Isaac and Oliver, respectively      |
| Week 4: 4/15 - 4/21 |   First milestone goals (see above)   |     Team effort     |
| Week 5: 4/22 - 4/26 |   Final goals (see above)   |        Team effort   |

### 12. Proposal Presentation

Add your slides to the Final Project Proposal slide deck in the Google Drive.

## Final Project Report

Don't forget to make the GitHub pages public website!
If you’ve never made a Github pages website before, you can follow this webpage (though, substitute your final project repository for the Github username one in the quickstart guide):  <https://docs.github.com/en/pages/quickstart>

### 1. Video

[Insert final project video here]

### 2. Images

[Insert final project images here]

### 3. Results

What were your results? Namely, what was the final solution/design to your problem?

#### 3.1 Software Requirements Specification (SRS) Results

Based on your quantified system performance, comment on how you achieved or fell short of your expected software requirements. You should be quantifying this, using measurement tools to collect data.

#### 3.2 Hardware Requirements Specification (HRS) Results

Based on your quantified system performance, comment on how you achieved or fell short of your expected hardware requirements. You should be quantifying this, using measurement tools to collect data.

### 4. Conclusion

Reflect on your project. Some questions to consider: What did you learn from it? What went well? What accomplishments are you proud of? What did you learn/gain from this experience? Did you have to change your approach? What could have been done differently? Did you encounter obstacles that you didn’t anticipate? What could be a next step for this project?

## References

Fill in your references here as you work on your proposal and final submission. Describe any libraries used here.

## Github Repo Submission Resources

You can remove this section if you don't need these references.

* [ESE5160 Example Repo Submission](https://github.com/ese5160/example-repository-submission)
* [Markdown Guide: Basic Syntax](https://www.markdownguide.org/basic-syntax/)
* [Adobe free video to gif converter](https://www.adobe.com/express/feature/video/convert/video-to-gif)
* [Curated list of example READMEs](https://github.com/matiassingers/awesome-readme)
* [VS Code](https://code.visualstudio.com/) is heavily recommended to develop code and handle Git commits
  * Code formatting and extension recommendation files come with this repository.
  * Ctrl+Shift+V will render the README.md (maybe not the images though)
