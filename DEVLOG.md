# PRICE PREDICTION SERVER
Development Log (DEVLOG)

Course: CME 3205 – Operating Systems
Semester: 2025–2026 Fall
Student: ........................................
Student No: .....................................
Group No (if any): ..............................

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

The following global limits and constants will be strictly used:

- PORT_NUMBER = 60000
- MAX_SAMPLES = 10000
- MAX_FEATURES = 100
- STRING_BUFFER_LIMIT = 100
- PREPROC_THREAD_LIMIT = 128
- COEFF_THREAD_LIMIT = 128

--------------------------------------------------
## 4. DEVELOPMENT PHILOSOPHY

- The project will be developed in clearly separated stages.
- Each stage will be completed, tested, and documented before moving to the next.
- No external ML or math libraries will be used.
- All matrix operations and regression solvers will be implemented manually in C.
- Concurrency will be visible, controlled, and well-documented.

--------------------------------------------------
## 5. INSTRUCTOR-PROVIDED REFERENCE FILES

The following files were provided by the instructor as references:

- Basic socket programming examples
- POSIX thread creation and synchronization examples
- CSV file parsing examples

These files are used strictly as:
- Structural reference
- Syntax reference
- Design guidance

They are NOT copied directly.
All implementations in this project are rewritten and integrated into the final architecture.

--------------------------------------------------
## 6. CURRENT PROJECT STATUS

### ✅ RESET PHASE COMPLETED
- Decision taken to reset the project and restart with a clean architecture.
- DEVLOG.md file created as the main development tracking document.
- Development is restarted with a structured, step-by-step plan.

--------------------------------------------------
## 7. DEVELOPMENT ROADMAP

### STAGE 1 – Core Socket Server
- TCP socket server on port 60000
- Single-client blocking accept
- Telnet-compatible input/output
- CSV filename input handling

Status: ✅ COMPLETED

**Technical Summary:**
Implemented a production-style TCP socket server using POSIX sockets. The server binds to port 60000 with SO_REUSEADDR enabled for address reuse. Uses blocking accept() to handle one client connection at a time. On client connection, sends welcome message, prompts for CSV filename, reads one line of input, logs the filename to server console, sends confirmation message, and closes the connection. Server runs indefinitely, accepting new connections in a loop. All socket operations include proper error handling using perror().

---

### STAGE 2 – CSV File Presence Check & Basic Analysis
- File existence check
- Column count detection
- Row count detection
- Error handling for missing datasets

Status: ✅ COMPLETED

**Technical Summary:**
Implemented analyze_csv() function that performs file existence validation using fopen(). If file is not found, sends error message to client and logs to server console. If file exists, reads the CSV file line by line. Column count is determined by counting commas in the first line (header row) plus one. Row count is determined by counting all remaining data lines after the header. Results are printed on server console and sent to the connected telnet client. Function integrates seamlessly into the existing socket server flow, called after receiving the CSV filename from the client.

---

### STAGE 3 – Column Type Detection (Schema Builder)
- Automatic detection of numeric vs categorical attributes
- Target column (price or equivalent) detection

Status: ✅ COMPLETED

**Technical Summary:**
Implemented automatic schema detection system that analyzes CSV structure and column types. The analyze_csv() function reads the header row and first data row to determine column characteristics. Column type detection uses strtod() parsing on first data row values to classify columns as numeric or categorical. Target column detection performs case-insensitive substring matching on header names for "price", "target", or "label" keywords. All schema information is stored in global arrays: column_names[], column_types[], target_column_index, total_rows, and total_columns. The schema builder function is fully network-independent (no send() calls), with all client communication handled in main(). Results are printed to server console and sent to telnet client in the correct order.

---

### STAGE 4 – Multi-Threaded Min–Max Normalization
- One thread per numeric attribute
- Compute xmin and xmax
- Apply min–max normalization
- Store normalization parameters

Status: ✅ COMPLETED

**Technical Summary:**
Implemented multi-threaded min-max normalization system where each numeric column is processed by a dedicated thread. Each thread scans its assigned column over all rows to compute xmin (minimum value) and xmax (maximum value). The normalization formula x_norm = (x - xmin) / (xmax - xmin) is then applied to all values in that column, with special handling for zero-range columns (range = 1.0 to avoid division by zero). Normalized values are stored in normalized_data[][] matrix, while xmin[] and xmax[] arrays preserve the normalization parameters for later reverse-normalization. Threads operate independently on their assigned columns with no mutex required since each thread writes to a different column. This parallel processing architecture ensures efficient normalization of all numeric attributes simultaneously.

---

### STAGE 5 – Multi-Threaded Categorical Encoding
- yes/no → 1/0 encoding
- Housing.csv special furnishingstatus encoding:
  - furnished → 2
  - semi-furnished → 1
  - unfurnished → 0

Status: ✅ COMPLETED

**Technical Summary:**
Implemented multi-threaded categorical encoding system where each categorical column is processed by a dedicated thread. The load_csv_data() function stores original categorical string values in raw_categorical[][][] buffer during initial CSV loading, eliminating redundant disk I/O. Each encoding thread reads directly from in-memory raw_categorical buffer instead of re-reading the CSV file. Encoding rules: yes/no values map to 1/0, and Housing.csv furnishingstatus column uses special mapping (furnished→2, semi-furnished→1, unfurnished→0). All encoded numeric values are stored in encoded_data[][] matrix. Threads operate independently on their assigned columns with no mutex required since each thread writes to a different column. This architecture ensures efficient in-memory preprocessing without redundant file operations.

---

### STAGE 6 – Design Matrix Construction
- Combine all normalized and encoded features into X_norm matrix
- Build y_norm target vector

Status: ✅ COMPLETED

**Technical Summary:**
Implemented design matrix construction that combines normalized numeric features and encoded categorical features into a unified feature matrix X_norm. The matrix is constructed column by column: Column 0 contains the bias term (1.0 for all rows), followed by all normalized numeric attributes excluding the target column (preserving original column order), then all encoded categorical attributes. The target vector y_norm is built by extracting normalized values from the target column index. The build_design_matrix() function iterates through all rows, systematically populating X_norm and y_norm using the existing normalized_data and encoded_data matrices. Original column order is preserved, and the target column is correctly excluded from X_norm while being used to construct y_norm. All operations are performed sequentially after normalization and encoding threads complete, ensuring data consistency.

---

### STAGE 7 – Multi-Threaded Regression Training
- Normal Equations method
- One thread per regression coefficient βj
- Compute XᵀX and Xᵀy
- Solve using Gaussian Elimination

Status: ✅ COMPLETED

**Technical Summary:**
Implemented multi-threaded regression training using Normal Equations method. The system computes XᵀX (matrix A) and Xᵀy (vector B) using one thread per regression coefficient βj. Each thread calculates its assigned row of A and its assigned element of B by performing dot products: A[j][k] = Σ(X_norm[i][j] * X_norm[i][k]) and B[j] = Σ(X_norm[i][j] * y_norm[i]) over all rows i. After all coefficient threads complete and join, the main thread solves the linear system (XᵀX)β = Xᵀy using Gaussian Elimination with partial pivoting. The solution vector β is stored in beta[] array. The system outputs the complete normalized regression equation showing β0 (bias term) and all βj coefficients with their corresponding feature names. Thread safety is ensured as each thread writes only to its own row of A and its own element of B, requiring no mutex synchronization.

---

### STAGE 8 – Interactive Prediction System
- Read raw user input
- Normalize numeric values
- Encode categorical values
- Compute ŷ_norm
- Reverse-normalize prediction to real scale

Status: ✅ COMPLETED

**Technical Summary:**
Implemented interactive prediction system that accepts user input via telnet after training completes. The system prompts for each feature value sequentially, starting from feature index 1 (excluding bias term). For numeric features, raw values are read and normalized using the stored xmin[] and xmax[] parameters with the formula x_norm = (x - xmin) / (xmax - xmin). For categorical features, string input is read and encoded using the same rules as training: yes/no maps to 1/0, and Housing.csv furnishingstatus uses special mapping (furnished→2, semi-furnished→1, unfurnished→0). A temporary X_input_norm[] vector is constructed with bias term 1.0 at index 0 and normalized/encoded values at subsequent indices. The normalized prediction ŷ_norm is computed as the dot product Σ(beta[j] * X_input_norm[j]) over all features. Reverse normalization is applied to convert to real scale: ŷ_real = ŷ_norm * (xmax[target] - xmin[target]) + xmin[target]. Both normalized and real-scale predictions are sent to the client. The system uses existing beta[], xmin[], xmax[], feature_to_column[], and column_types[] arrays without recalculating training parameters.

--------------------------------------------------
## 8. BUILD & EXECUTION

Compilation:
```bash
gcc assignment.c -o assignment -lpthread

Execution:
./assignment

Client Connection:
telnet localhost 60000

##9. ACADEMIC INTEGRITY NOTE

This project is developed manually in C.
Online resources and AI-based tools are used strictly as:

Syntax helpers

Debugging assistants

Design discussion tools

All final implementations are fully understood and owned by the student(s).
