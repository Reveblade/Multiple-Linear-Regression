# Multi-Threaded Price Prediction Server (C / Linux)

This project is a multi-threaded TCP Price Prediction Server implemented in C for Linux.
It loads CSV housing data, performs parallel preprocessing, trains a Multiple Linear Regression
model on normalized attributes, and serves real-time price predictions to clients over telnet.

--------------------------------------------------
## 1. FEATURES

- TCP socket server on port 60000
- Telnet-compatible input/output
- Automatic CSV schema detection:
  - Numeric vs categorical attributes
  - Target column detection (price / target / label)
- Multi-threaded preprocessing:
  - Min–max normalization for numeric attributes
  - Attribute-level categorical encoding for string attributes
- Special handling for Housing.csv:
  - furnishingstatus:
    - furnished       → 2
    - semi-furnished  → 1
    - unfurnished     → 0
- Multi-threaded regression training:
  - Normal Equations: (XᵀX)β = Xᵀy
  - One thread per coefficient row
  - Gaussian Elimination solver
- Interactive prediction mode:
  - User enters raw feature values via telnet
  - System normalizes/encodes inputs, predicts normalized price,
    and converts it back to real-scale price.

--------------------------------------------------
## 2. FOLDER STRUCTURE

Project root (example):

- config/
  - Housing.csv
- src/
  - main.c
  - server.c
  - server.h
  - csv.c
  - csv.h
  - preprocess.c
  - preprocess.h
  - regression.c
  - regression.h
  - prediction.c
  - prediction.h
- DEVLOG.md
- README.md

Note: Depending on the final submission format, assignment.c can act as a single
entry point that includes the module headers and links all source files.

--------------------------------------------------
## 3. BUILD INSTRUCTIONS

Example compilation command (single binary named "assignment"):

- gcc assignment.c -o assignment -lpthread

If the project is compiled module by module, a possible command is:

- gcc *.c -o assignment -lpthread

The exact command may be adjusted depending on how the instructor expects the project to be built.

--------------------------------------------------
## 4. EXECUTION

1. Start the server:

   - ./assignment

2. From another terminal (same machine), connect via telnet:

   - telnet localhost 60000

3. When prompted:

   - Enter CSV file name to load:
   - Type: Housing.csv

   The server internally resolves this to:

   - config/Housing.csv

4. After loading and training, the server will show the learned normalized regression equation
   and then switch to prediction mode.

--------------------------------------------------
## 5. EXAMPLE INTERACTION

Terminal 1 (server):

- ./assignment

Terminal 2 (client via telnet):

- telnet localhost 60000

Example flow (simplified):

- WELCOME TO PRICE PREDICTION SERVER
- Enter CSV file name to load:
- Housing.csv

The client then sees:

- rows/columns count
- column analysis
- normalization and encoding logs
- training logs
- final regression equation

Then the client is prompted:

- Enter values for prediction:
- Enter area:
- Enter bedrooms:
- ...

After entering all features, the server replies with something like:

- Prediction Result:
- Normalized: 0.550597
- Real Scale: 8109400.341835

Finally the server closes the connection:

- Closing connection...

--------------------------------------------------
## 6. DATASET NOTES

The default dataset is:

- config/Housing.csv

Column roles (as detected automatically):

- Numeric:
  - area
  - bedrooms
  - bathrooms
  - stories
  - parking
  - price (TARGET)
- Categorical:
  - mainroad
  - guestroom
  - basement
  - hotwaterheating
  - airconditioning
  - prefarea
  - furnishingstatus

Categorical encoding:

- yes / no → 1 / 0
- furnishingstatus (only for Housing.csv):
  - furnished       → 2
  - semi-furnished  → 1
  - unfurnished     → 0
  - others / empty  → 0

--------------------------------------------------
## 7. LIMITATIONS

- Only one client is handled at a time (blocking accept model).
- The implementation assumes a well-formed CSV file with a header row.
- Maximum samples and features are bounded by compile-time constants:
  - MAX_SAMPLES = 10000
  - MAX_FEATURES = 100
- The Normal Equations method may be numerically sensitive for very ill-conditioned matrices,
  but it is sufficient for the scale of the given assignment datasets.

--------------------------------------------------
## 8. AUTHORS

- Mert YILDIZ — 202150071
- Doğukan DAĞLI — 2022510193

--------------------------------------------------
## 9. ACADEMIC NOTE

This project is developed for educational purposes as part of:

- CME 3205 – Operating Systems
- 2025–2026 Fall Semester

Code is written manually in C. External resources and AI tools are used only as
syntax helpers, debugging assistants, and design discussion tools. All final code
and design decisions are understood and owned by the authors.
