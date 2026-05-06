# LSwarm Mk.1
Micro-drones for a light swarm

![3D PCB](image-1.png)

![PCB](image.png)

![Schematic](image-2.png)
## Introduction
As of recently, drones are becoming a greater and greater commodity around the world. They serve a multitude of purposes in areas like photography and entertainment, often adding to the capabilities of that field.

Notably, one capability that many have a lack of access to are **3D (volumetric) displays**, often made from drones with expensive GPS systems and control systems. A swarm of these drones could cost **thousands of dollars** for even a basic display. 

The goal of the LSwarm Mark 1 is to provide a **cheap** alternative, approachable to everyone. Additionally, its all open sourced under the GNU GPL license! (do what you will with the code and designs but make sure to keep it open-source! :D)

### How will it localize without GPS?
Although the LS Mk.1 is still **experimental** (haven't even written the code yet), using a powerful IMU and barometer, the drones should be able to localize their position **accurately within a few centimeters**. Although sensor drift is a concern, I believe it could be minimized with proper calibration and a good startup sequence.

### Are drone swarms safe and legal?
Without a permit, flying more than one drone by yourself **outside** is **illegal** in most countries (especially the U.S.). However, in the U.S., flying **indoors** is technically not uncontrolled airspace and so FAA rules do not apply to it. Hence, in the U.S., **drone swarms** are completely **legal** as long as you **fly indoors** and **maintain safety precautions**. (Note: this is not legal advise, please check with your local government's rules and guidelines)


## Bill of Materials 

#### (Minimum build for testing: 5 drones)

| Quantity | Components | Price | Link |
|--------- |----------|----------| -----|
| 20  | Brushed DC Motors | ~$9.68 | [here](https://www.aliexpress.us/item/3256810271706869.html?spm=a2g0o.productlist.main.48.18214690sK1aRt&algo_pvid=53df9bc5-14d9-4ef0-b69f-11a93d529ec5&algo_exp_id=53df9bc5-14d9-4ef0-b69f-11a93d529ec5-45&)
| 5  | 1s 450mAh BT2.0 Batteries | ~$11.21 | [here](https://www.aliexpress.us/item/3256810349502140.html?spm=a2g0o.productlist.main.9.7055TeWnTeWnyW&algo_pvid=e87d392e-a216-4ba3-8956-baf637a8cba3&algo_exp_id=e87d392e-a216-4ba3-8956-baf637a8cba3-8&pdp_ext_f=%7B%22order%22%3A%2248%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21USD%2113.57%216.78%21%21%2192.34%2146.17%21%402103110517771681621716757e4b37%2112000052738741335%21sea%21US%217493938711%21X%211%210%21n_tag%3A-29913%3Bd%3Ad260408f%3Bm03_new_user%3A-29895&curPageLogUid=6cUgOJiOwfNW&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005010535816892%7C_p_origin_prod%3A)
| 1 | 1s BT2.0 Charger | ~18.18 | [here](https://www.aliexpress.us/item/3256811772590472.html?spm=a2g0o.productlist.main.3.315a3kxV3kxVU5&algo_pvid=a57de163-f3db-4a1d-966e-e88601147021&algo_exp_id=a57de163-f3db-4a1d-966e-e88601147021-2&pdp_ext_f=%7B%22order%22%3A%22-1%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21USD%2131.82%2119.09%21%21%21216.43%21129.86%21%402101f54117771690026201319e9264%2112000057143664294%21sea%21US%217493938711%21X%211%210%21n_tag%3A-29913%3Bd%3Ad260408f%3Bm03_new_user%3A-29895&curPageLogUid=pie8lE5eVKgd&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005011958905224%7C_p_origin_prod%3A) 
| 5  | Mk.1 PCBs | ~$150 (with shipping, cost can vary widely however) | Download repo, extract fabrication files, and fabricate through JLCPCB (complete instructions for this may come soon)

Total cost: 189.07

Cost per drone: $37.814

----

#### (Minimum build for display: ~20 drones)

| Quantity | Components | Price | Link |
|--------- |----------|----------| -----|
| 80  | Brushed DC Motors | ~$38.72 | [here](https://www.aliexpress.us/item/3256810271706869.html?spm=a2g0o.productlist.main.48.18214690sK1aRt&algo_pvid=53df9bc5-14d9-4ef0-b69f-11a93d529ec5&algo_exp_id=53df9bc5-14d9-4ef0-b69f-11a93d529ec5-45&)
| 20  | 1s 450mAh BT2.0 Batteries | ~$44.84 | [here](https://www.aliexpress.us/item/3256810349502140.html?spm=a2g0o.productlist.main.9.7055TeWnTeWnyW&algo_pvid=e87d392e-a216-4ba3-8956-baf637a8cba3&algo_exp_id=e87d392e-a216-4ba3-8956-baf637a8cba3-8&pdp_ext_f=%7B%22order%22%3A%2248%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21USD%2113.57%216.78%21%21%2192.34%2146.17%21%402103110517771681621716757e4b37%2112000052738741335%21sea%21US%217493938711%21X%211%210%21n_tag%3A-29913%3Bd%3Ad260408f%3Bm03_new_user%3A-29895&curPageLogUid=6cUgOJiOwfNW&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005010535816892%7C_p_origin_prod%3A)
| 2 | 1s BT2.0 Charger | ~36.36 | [here](https://www.aliexpress.us/item/3256811772590472.html?spm=a2g0o.productlist.main.3.315a3kxV3kxVU5&algo_pvid=a57de163-f3db-4a1d-966e-e88601147021&algo_exp_id=a57de163-f3db-4a1d-966e-e88601147021-2&pdp_ext_f=%7B%22order%22%3A%22-1%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&pdp_npi=6%40dis%21USD%2131.82%2119.09%21%21%21216.43%21129.86%21%402101f54117771690026201319e9264%2112000057143664294%21sea%21US%217493938711%21X%211%210%21n_tag%3A-29913%3Bd%3Ad260408f%3Bm03_new_user%3A-29895&curPageLogUid=pie8lE5eVKgd&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005011958905224%7C_p_origin_prod%3A) 
| 20  | Mk.1 PCBs | ~$250 (with shipping, cost can vary widely however) | Download repo, extract fabrication files, and fabricate through JLCPCB (complete instructions for this may come soon)

Total cost: 369.92

Cost per drone: $18.49

----