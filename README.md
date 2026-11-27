Exhale

An interactive breathing practice that you can see. 

1. Creative motivation and intended use

Why I made it: 
Slow, controlled breathing is one of the simplest skills for settling the body, but people tend to forget to do it when they're busy or stressed. I wanted to create something that can sort of remind users to take deep breathes more often as it can be very useful skill and task to regulate user's physical and mental health. I wanted it to be a visible result as it is more fun, allowing users to want to come back to it and turning a long, steady exhale into something you can watch and control.

How the concept developed:
My first idea was to measure heart rate with a pulse sensor and reward the player for calming down. After testing and getting feedback from my instructor, I dropped it: finger-worn sensors proved unreliable, and heart rate responds to breathing too slowly (often a minute or more) to give a player useful feedback. Blowing gives instant, continuous feedback, so it is a better fit for for my interactive project. It also trains the part of deep breathing that matters most, a slow and even exhale.

Who I envision using it:
Anyone who wants to practise slower breathing in a short, low-pressure session: students during exam periods, visitors to a wellbeing room or library, or children and adults being introduced to breathing exercises. It is designed in a way that needs no further instructions beyond the on-screen guide.

What I want the audience to experience:
Calm and curiosity, not competition. The player breathes in, then blows out slowly toward the sensor, and that rewards an interaction: [the flower blooms / the light grows steadier / the balloon rises]. A hard, short blow wont make it work, so the player naturally learns to ease off. I deliberately avoid interactions on the screen that are too active because that could make people tense, which would defeat the purpose. 

2. Implementation

It's a small device. An ESP32 microcontroller reads a microphone sensor and draws animated scenes on a 1.8" colour TFT screen, so no laptop is needed while it runs. The whole project is programmed in C++ using the Arduino IDE.

1. **Sensing the breath.** A microphone sound-sensor module is connected to an analog pin (GPIO 34) on the ESP32. When the player blows toward it, the airflow makes a low rumble that the microphone picks up. The code samples the microphone as fast as it can for 40 milliseconds, then takes the difference between the highest and lowest reading in that window. That difference is the "loudness" of the breath, and it is printed to the Serial Monitor so the sensitivity can be tuned.
  
2. **Recognising a deliberate exhale.** A single loud spike (a tap, a cough, a pop) should not count as a breath. A blow only counts if the loudness stays above a threshold for at least 500 ms. Short gaps are forgiven: 150 ms before a breath is accepted, and 300 ms once it has been accepted, so an uneven exhale does not cut the animation off.

3. **Three breath-controlled scenes.** The breath is used in a different way in each scene, so the player practises slightly different kinds of breathing:
   - **Fish: one long, steady exhale.** The fish swims toward a cave only while the player keeps exhaling, and stops (without losing its progress) when they pause. It needs roughly 3.5 seconds of total exhale to arrive, kicking up sand as it swims, then curls up and sleeps.
   - **Puppy: three separate breaths.** Each deliberate exhale blows a wave of flower petals off the screen. After three breaths the petals are gone, revealing a puppy with its head tilted. Three dots at the bottom count the breaths.
   - **Lamp: a sustained blow that builds light.** The lamp glows brighter, the warm light spreads across the room, and the stars fade as blowing time accumulates (about 3.6 seconds in total). When the lamp is fully lit, a message appears: "Remember to take deep breaths :)".
5. **Looping.** After each scene finishes there is a 3-second pause, then the next one begins (fish, puppy, lamp, then back to the fish), so the piece runs continuously for the next visitor.
6. **Teaching prompts.** Short on-screen messages guide the breathing: "Exhale to swim", "Take a deep breath and exhale...", "Breathe in", and "Let the light grow". They change depending on what the player is doing, for example "Breathe in" appears between exhales in the lamp scene.

### Generative and procedural techniques

All of the artwork is drawn from code each frame, using shapes such as circles, triangles, rounded rectangles and lines. There are no image files and no pre-made animation frames. This keeps the program small enough for a microcontroller and means every visual detail can respond to the breath.

- **Parametric animation:** the scene is recalculated every frame from the current breath state and the clock. The fish's position, the lamp's light radius, and the petals' flight are all driven by variables such as `fishX`, `sceneProgress` and `dogBreaths`.
- **Sine-wave motion:** gentle oscillation makes the world feel alive. It drives the rising bubbles, swaying seaweed, drifting clouds, the fish's tail, the dog's wagging tail, and twinkling stars.
- **Colour blending:** a function interpolates between two colours in the screen's 16-bit format. It is used to warm the lamp shade and build up layered glow rings as the player blows, so the light seems to grow gradually.

Why these choices

- A microphone as a breath sensor: A blow makes a clear burst of low-frequency noise, so an inexpensive microphone module can detect how long someone breathes out, without anything touching the player.
- Different scenes for different breaths: The fish rewards a long, steady exhale, the puppy rewards repeated deep breaths, and the lamp rewards sustained blowing. Together they practise the skills involved in slow breathing.
- Progress is kept, not reset: If the player stops, the fish stays where it is and the lamp stays lit, so nobody "fails". This keeps the piece calm and encourages gentle, repeated attempts.
- Friendly illustrated scenes: Characters such as a sleeping fish and a puppy make the exercise feel playful and suit a wide audience, including children.

### Software and libraries

- Arduino IDE 

### Hardware

- ESP32 development board 
- Microphone sound-sensor module with analog output, GPIO 34
- 1.8" ST7735 TFT screen, 160 x 128 pixels
- Jumper wires, USB cable, 3D printed case 

  
3. Evaluation: how successful was it?


What works well: Had a lot of trial and error with this project regarding like the distance of where I should start blowing, or how far it stops detecting my blow, how hard I want users to blow etc. I think what worked well in the end is that users don't have to blow really hard for it to work, just slow calm exhales work really well which helps users to calm down. 

Challenges I faced: At first I wanted to use the ultrasonic sensor and a movable object but it didn't work. I had a paper flap stuck onto a toothpick and when I blowed the end of the paper flap would move backwards and the ultrasonic sensor would detect motion, however it did not work well at all as it could not really detect the movement, too far and it wont detect and too close it wouldn't detect either so I resulted in using the microphone instead. 


What I would change with more time: I would perhaps print a different casing for it to cover the microphone bit a bit more and try adding slightly more games to it not just 3. 


4. Credits and originality


My own work

The concept, the research into related breathing and biofeedback projects, and the decision to move from a pulse sensor to a breath-driven design
The physical build and wiring 
The calibration, smoothing and breath-mapping logic
The generative visuals, guide animation, text prompts and level design


5. How to compile and run

The project is a single Arduino sketch that runs on an ESP32. No laptop is needed once the sketch is uploaded.

The device can be powered from the computer or any USB power bank.

How to use it

1. Plug the device into a type c portable charger or a type c charger. Hold the device at around  [5-10] cm, angled slightly so your breathe can be detected by the microphone.
2. Breathe in, then exhale slowly and steadily toward the microphone.
3. Follow the on-screen prompts through the three scenes: the fish swimming to its cave, the puppy and the petals, and the lamp. The scenes repeat in a loop.

