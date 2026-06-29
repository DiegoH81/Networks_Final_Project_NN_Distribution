# Commented out IPython magic to ensure Python compatibility.
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.utils.data import DataLoader, TensorDataset, random_split

import torch.optim as optim
import torch.distributions as dist

import matplotlib.pyplot as plt
from sklearn.metrics import confusion_matrix, ConfusionMatrixDisplay, classification_report
import numpy as np

import pandas as pd
import os 

import torch.distributions as dist

import udp_module

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




torch.manual_seed(42)

input_dim = 14
num_classes = 3
batch_size = 100


# Load dataset from CSV

current_dir = os.path.dirname(os.path.abspath(__file__))
csv_path = os.path.join(current_dir, "..", "data", "Dataset of Diabetes.csv")


# CPP
number_slaves = 3

Master_CPP = udp_module.MasterUDP(45000, number_slaves, False)
Master_CPP.register_slaves()
csv_block = Master_CPP.prepare_and_send_dataset(csv_path)

df = pd.read_csv(pd.io.common.StringIO(csv_block), header = None)


X_np = df.iloc[:, :input_dim].values.astype(np.float32)
y_np = df.iloc[:, -num_classes:].values.astype(np.float32)

X = torch.tensor(X_np)
y = torch.tensor(y_np)

# Create dataset and split into training/testing
dataset = TensorDataset(X, y)

train_size = int(0.8 * len(dataset))
test_size = len(dataset) - train_size
train_dataset, test_dataset = random_split(dataset, [train_size, test_size])

train_loader = DataLoader(train_dataset, batch_size = batch_size, shuffle = True)
test_loader = DataLoader(test_dataset, batch_size  = batch_size, shuffle = False)

# Model setup
model = MulticlassClassifier(input_dim = input_dim, num_classes = num_classes)

data_init = model.fc1.weight.data[0:6].clone()

criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr = 0.001)

# Training loop
train_tracker, test_tracker, accuracy_tracker = [], [], []

model.train()
epoch_loss = 0
counter = 0

for batch_x, batch_y in train_loader:
    optimizer.zero_grad()

    if counter > 0:
        Master_CPP.send_weights_to_slaves(counter, 1, model.fc1.weight.data.tolist())
        Master_CPP.send_weights_to_slaves(counter, 2, model.fc2.weight.data.tolist())
        Master_CPP.send_weights_to_slaves(counter, 3, model.fc3.weight.data.tolist())
        Master_CPP.send_weights_to_slaves(counter, 4, model.fc4.weight.data.tolist())

        sum1 = Master_CPP.receive_weights_from_slaves(counter, 1)
        sum2 = Master_CPP.receive_weights_from_slaves(counter, 2)
        sum3 = Master_CPP.receive_weights_from_slaves(counter, 3)
        sum4 = Master_CPP.receive_weights_from_slaves(counter, 4)
        
        avg1 = udp_module.average_weights(model.fc1.weight.data.tolist(), sum1, number_slaves + 1)
        avg2 = udp_module.average_weights(model.fc2.weight.data.tolist(), sum2, number_slaves + 1)
        avg3 = udp_module.average_weights(model.fc3.weight.data.tolist(), sum3, number_slaves + 1)
        avg4 = udp_module.average_weights(model.fc4.weight.data.tolist(), sum4, number_slaves + 1)
        
        model.fc1.weight.data.copy_(torch.tensor(avg1))
        model.fc2.weight.data.copy_(torch.tensor(avg2))
        model.fc3.weight.data.copy_(torch.tensor(avg3))
        model.fc4.weight.data.copy_(torch.tensor(avg4))
        

    logits, log_vars = model(batch_x)
    loss = criterion(logits, batch_y)
    loss.backward()
    optimizer.step()

    epoch_loss += loss.item()
    counter += 1

Master_CPP.send_end()


print("\n--- Initial weights FC1 ---")
print(data_init)
print("-" * 30)

print("\n--- Final weights FC1 ---")
print(model.fc1.weight.data[0:6])
print("-" * 30)


train_tracker.append(epoch_loss / len(train_loader))




# Evaluation on test set
y_true = []
y_pred = []
log_vars_all = []


#with torch.no_grad():
test_loss = 0
total = 0
num_correct = 0
for batch_x, batch_y in test_loader:
    logits, log_vars = model(batch_x)
    loss = criterion(logits, batch_y)
    test_loss += loss.item()

    predictions = torch.argmax(logits, dim=1)
    total += batch_x.size(0)
    num_correct += (predictions == torch.argmax(batch_y, dim=1)).sum().item()

    predictions = torch.argmax(logits, dim=1)
    y_true.extend(torch.argmax(batch_y, dim=1))
    y_pred.extend(predictions.tolist())
    log_vars_all.append(log_vars)

test_tracker.append(test_loss/len(test_loader))
print(f"Test loss: {test_loss/len(test_loader)} | ", end='')
accuracy_tracker.append(num_correct/total)
print(f'Accuracy : {num_correct/total}')


# Plot training loss over epochs
plt.figure(figsize=(8, 4))
plt.plot(train_tracker, marker='o')
plt.title("Training Loss Over Epochs")
plt.xlabel("Epoch")
plt.ylabel("Loss")
plt.grid(True)
plt.tight_layout()
plt.show()


# Mat plot lib
plt.plot(train_tracker, label='Training loss')
plt.plot(test_tracker, label='Test loss')
plt.plot(accuracy_tracker, label='Test accuracy')
plt.legend()


# Display confusion matrix
cm = confusion_matrix(y_true, y_pred)
disp = ConfusionMatrixDisplay(confusion_matrix=cm, display_labels=list(range(num_classes)))
disp.plot(cmap=plt.cm.Blues)
plt.title("Confusion Matrix")
plt.tight_layout()
plt.show()

# Print classification metrics
print("\nClassification Report:")
print(classification_report(y_true, y_pred, digits=3))