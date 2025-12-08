# PRICE PREDICTION SERVER
Development Log (DEVLOG)

Course: CME 3205 – Operating Systems
Semester: 2025–2026 Fall

Student 1: Mert YILDIZ
Student No: 202150071

Student 2: Doğukan DAĞLI
Student No: 2022510193

--------------------------------------------------
## 1. PROJECT DESCRIPTION

This project is a multi-threaded Price Prediction Server implemented in C for Linux.
The system loads CSV datasets, preprocesses numerical and categorical features using
multi-threading, trains a Multiple Linear Regression (MLR) model on normalized data,
and serves predictions to clients over a TCP socket connection using telnet.

The server:
- Listens on a fixed TCP port (60000)
- Accepts client connections via telnet
- Loads and analyzes a selected CSV dataset
- Applies min–max normalization
- Encodes categorical attributes using predefined rules
- Computes regression coefficients using multi-threaded computation
- Accepts user input for prediction
- Outputs both normalized and real-scale predictions

--------------------------------------------------
## 2. PROJECT OBJECTIVES

- Implement a Linux-based TCP socket server in C.
- Perform dataset preprocessing using POSIX threads.
- Apply correct min–max normalization to numeric attributes.
- Apply correct categorical encoding rules, including special rules for Housing.csv.
- Train a Multiple Linear Regression model on normalized data.
- Use threads for both preprocessing and coefficient computation.
- Ensure full telnet-based client interaction.
- Demonstrate correct synchronization without data races or deadlocks.
- Produce correct normalized and reverse-normalized predictions.

--------------------------------------------------
## 3. GLOBAL CONSTRAINTS (FROM ASSIGNMENT)

The following global limits and constants are strictly applied:

- PORT_NUMBER = 60000
- MAX_SAMPLES = 10000
- MAX_FEATURES = 100
- STRING_BUFFER_LIMIT = 100
- PREPROC_THREAD_LIMIT = 128
- COEFF_THREAD_LIMIT = 128

--------------------------------------------------
## 4. DEVELOPMENT PHILOSOPHY

- The project is developed in clearly separated stages.
- Each stage is completed, tested, and documented before moving to the next.
- No external ML or math libraries are used.
- All matrix operations and regression solvers are implemented manually in C.
- Concurrency is visible, controlled, and well-documented.

--------------------------------------------------
## 5. INSTRUCTOR-PROVIDED REFERENCE FILES

The following files were provided by the instructor as references:

- Basic socket programming examples
- POSIX thread creation and synchronization examples
- CSV file parsing examples

These resources are used strictly as:
- Structural reference
- Syntax reference
- Design guidance

All final implementations are rewritten and integrated manually.

--------------------------------------------------
## 6. CURRENT PROJECT STATUS

- RESET PHASE COMPLETED
- ALL DEVELOPMENT STAGES COMPLETED SUCCESSFULLY
- SYSTEM FULLY OPERATIONAL
- MULTI-THREADING VERIFIED
- REAL-TIME TELNET PREDICTION VERIFIED

--------------------------------------------------
## 7. DEVELOPMENT ROADMAP & IMPLEMENTATION

### STAGE 1 – Core Socket Server (COMPLETED)

- Implemented a TCP socket server that binds to port 60000 using AF_INET/SOCK_STREAM.
- SO_REUSEADDR option is enabled to allow quick restart after crashes.
- The server uses blocking accept() to handle one client connection at a time.
- For each connection, it sends a welcome message, prompts for a CSV filename,
  reads one line of input from the client, and logs the filename on the server console.
- All socket operations are checked; errors are printed with perror().
- The server runs in an infinite loop and closes each client socket cleanly.

### STAGE 2 – CSV File Presence Check & Basic Analysis (COMPLETED)

- Implemented analyze_csv(const char* filename) which checks dataset existence via fopen.
- If the file cannot be opened, an error message is prepared for the client and server console.
- If the file exists, the function:
  - Reads the header line and counts commas to detect the number of columns.
  - Counts remaining data lines to determine the number of rows.
- The function does not depend on the network layer; it only updates:
  - total_rows
  - total_columns
- These values are then reported to the client from the main/server layer.

### STAGE 3 – Column Type Detection (Schema Builder) (COMPLETED)

- The first data row is used to detect column types.
- For each column:
  - If strtod() can parse the value and there is no leftover string, the column is treated as NUMERIC.
  - Otherwise it is treated as CATEGORICAL.
- Column names from the header row are stored in column_names[].
- Target column detection:
  - Case-insensitive substring scan over header names for the keywords "price", "target" or "label".
  - If a match is found, target_column_index is set accordingly.
- This forms a complete schema:
  - column_names[]
  - column_types[]
  - target_column_index
  - total_rows
  - total_columns

### STAGE 4 – Multi-Threaded Min–Max Normalization (COMPLETED)

- Implemented normalize_column(void* arg) as a POSIX thread function.
- One thread is created per numeric column.
- Each thread:
  - Scans raw_data[0..total_rows-1][col] to compute xmin[col] and xmax[col].
  - Applies min–max normalization:
    - x_norm = (x - xmin) / (xmax - xmin)
  - If xmax == xmin (zero range), the range is set to 1.0 to avoid division by zero.
  - Writes normalized values into normalized_data[row][col].
- Threads only read their own column; no shared-write conflicts occur.
- After all normalization threads join, the server prints min and max values to both console and client.

### STAGE 5 – Multi-Threaded Categorical Encoding (COMPLETED)

- load_csv_data() keeps all original categorical fields in raw_categorical[row][col] buffers.
- Implemented encode_categorical_column(void* arg) as a thread function:
  - For each categorical column:
    - If the column is "furnishingstatus" and the dataset file name is Housing.csv,
      special encoding is applied:
        - furnished       → 2
        - semi-furnished  → 1
        - unfurnished     → 0
    - Otherwise, generic yes/no encoding is used:
        - "yes" → 1
        - "no"  → 0
        - any other / empty → 0
- Each thread writes encoded results into encoded_data[row][col] for its own column.
- No mutex is required because there is no concurrent write to the same memory region.
- The server logs the start of each categorical encoding thread to the client,
  and reports when all threads complete.

### STAGE 6 – Design Matrix Construction (COMPLETED)

- Implemented build_design_matrix() to assemble the full feature matrix X_norm and target vector y_norm.
- Feature ordering:
  - Feature 0 is the bias term: X_norm[row][0] = 1.0 for all rows.
  - Then all numeric features (except the target column) in original column order.
  - Then all categorical features in original column order.
- For each logical feature j:
  - feature_names[j] holds the readable name.
  - feature_to_column[j] maps back to the original CSV column index (or -1 for bias).
- The target vector y_norm[row] is taken from normalized_data[row][target_column_index].
- This stage guarantees a consistent mapping between:
  - X_norm
  - y_norm
  - feature_names[]
  - feature_to_column[]

### STAGE 7 – Multi-Threaded Regression Training (COMPLETED)

- Implemented compute_coefficient(void* arg) which operates per feature index j.
- For each j:
  - The thread computes row j of matrix A = XᵀX:
    - A[j][k] = Σ X_norm[i][j] * X_norm[i][k] over all rows i.
  - The same thread computes element B[j] of vector B = Xᵀy:
    - B[j] = Σ X_norm[i][j] * y_norm[i].
- After all coefficient threads join, the main thread solves the system:
  - A * beta = B
  - using Gaussian Elimination with partial pivoting.
- The solution vector beta[] contains all regression coefficients, including bias.
- The server prints the full normalized regression equation:
  - price_norm = beta0 + beta1 * feature1_norm + ... + betaN * featureN_norm

### STAGE 8 – Interactive Prediction System (COMPLETED)

- After training, the server enters prediction mode for the current client.
- For each feature j ≥ 1:
  - The client is prompted for a raw value.
  - For numeric features:
    - Input is converted to double and min–max normalized using stored xmin/xmax.
  - For categorical features:
    - The same encoding rules as in training are applied (yes/no or furnishingstatus).
- A temporary input vector X_input_norm[] is assembled:
  - X_input_norm[0] = 1.0 (bias)
  - X_input_norm[j] = normalized or encoded value
- The normalized prediction is computed as:
  - y_pred_norm = Σ beta[j] * X_input_norm[j]
- Reverse normalization is applied to convert to original price scale:
  - y_pred_real = y_pred_norm * (xmax[target] - xmin[target]) + xmin[target]
- Both normalized and real-scale predictions are sent back to the client.

--------------------------------------------------
## 8. BUILD & EXECUTION

Compilation (example):

- gcc assignment.c -o assignment -lpthread

Execution:

- ./assignment

Client connection from the same machine:

- telnet localhost 60000

When prompted for CSV file name, enter:

- Housing.csv

(The server resolves it to the config/Housing.csv path internally.)

--------------------------------------------------
## 9. ACADEMIC INTEGRITY NOTE

This project is developed manually in C.

External and AI-based tools are used only as:
- Syntax helpers
- Debugging assistants
- Design discussion tools

All final implementations, design decisions and code are fully understood
and owned by the student authors: Mert YILDIZ and Doğukan DAĞLI.
