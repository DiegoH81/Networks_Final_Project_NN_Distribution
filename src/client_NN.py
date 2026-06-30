# Commented out IPython magic to ensure Python compatibility.
import torch
import torch.nn as nn
import torch.nn.functional as F

import torch.optim as optim
import numpy as np
import pandas as pd

import udp_module


X = None
y = None

class MulticlassClassifier(nn.Module):
    def __init__(self, input_dim: int, num_classes: int, hidden1: int = 128, hidden2: int = 64):
        super(MulticlassClassifier, self).__init__()
        self.fc1 = nn.Linear(input_dim, hidden1)
        self.fc2 = nn.Linear(hidden1, hidden1)
        self.fc3 = nn.Linear(hidden1, hidden1)
        self.fc4 = nn.Linear(hidden1, hidden2)
        self.class_logits = nn.Linear(hidden2, num_classes)      # Predict class scores
        self.class_log_vars = nn.Linear(hidden2, num_classes)     # Predict log-variance for each class

    def forward(self, x: torch.Tensor):
        x = F.relu(self.fc1(x))
        x = F.relu(self.fc2(x))
        x = F.relu(self.fc3(x))
        x = F.relu(self.fc4(x))
        logits = self.class_logits(x)
        log_vars = self.class_log_vars(x)
        return logits, log_vars
        #return logits, logits



# Generate synthetic heteroscedastic multiclass data
torch.manual_seed(42)
num_samples = 1000
input_dim = 14
num_classes = 3



# ModelSetup
model = MulticlassClassifier(input_dim = input_dim, num_classes = num_classes)

criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr = 0.001)



# Setup
SlaveCPP = udp_module.SlaveUDP("127.0.0.1", 45000, True)
SlaveCPP.register_slave()

# Dataset
csv_data = SlaveCPP.receive_dataset()
df = pd.read_csv(pd.io.common.StringIO(csv_data), header=None)
X_np = df.iloc[:, :input_dim].values.astype(np.float32)
y_np = df.iloc[:, -num_classes:].values.astype(np.float32)
X = torch.tensor(X_np)
y = torch.tensor(y_np)



layers = [model.fc1, model.fc2, model.fc3, model.fc4]
current_weights = 0

# Training Loop
while True:
    batch_id, layer_id, matrix = SlaveCPP.receive_weights()

    if batch_id == -1:
        break

    layers[layer_id - 1].weight.data.copy_(torch.tensor(matrix))
    
    current_weights += 1
    if current_weights == len(layers):
        model.train()
        optimizer.zero_grad()
        logits, log_vars = model(X)
        loss = criterion(logits, y)
        loss.backward()
        optimizer.step()
        
        
        SlaveCPP.send_weights(batch_id, 1, model.fc1.weight.data.tolist())
        SlaveCPP.send_weights(batch_id, 2, model.fc2.weight.data.tolist())
        SlaveCPP.send_weights(batch_id, 3, model.fc3.weight.data.tolist())
        SlaveCPP.send_weights(batch_id, 4, model.fc4.weight.data.tolist())
        
        current_weights = 0