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



####  Get Data as string from protocol
data = np.array([[1,2,3,4,5,6,7,8,9,10,11,12,13,14]])

# Assume first 4 columns are input features, last 3 are one-hot class labels
X_np = data[:, : input_dim]
y_onehot_np = data[:, -num_classes: ]


X = torch.tensor(X_np)

# argmax?
y = torch.tensor(y_onehot_np)


#print("y ",y)
#print("y ",y.size())
#print("X ",X.size())


# ModelSetup
model = MulticlassClassifier(input_dim = input_dim, num_classes = num_classes)

criterion = nn.CrossEntropyLoss()
optimizer = torch.optim.Adam(model.parameters(), lr = 0.001)



# Load Model Data
dest_ip = "127.0.0.1"
port = 45000
client = udp_module.UdpClient(dest_ip, port)

# Hello msg
client.send_msg("HELLO")

# Training
while(True):
    model.train()

    optimizer.zero_grad()

    ####### Get  matrix
    ####### Load Matrix from MasterC to Master P

    last_msg = client.receive_latest_msg()
    print("Last msg:", last_msg)
    
    # new_matrix_1, new_matrix_2, new_matrix_3, new_matrix_4 = proto_get_matrix()
    # model.fc1.weight.data.copy_(torch.from_numpy(np.asarray(new_weights_matrix_1)))
    # model.fc2.weight.data.copy_(torch.from_numpy(np.asarray(new_weights_matrix_2)))
    # model.fc3.weight.data.copy_(torch.from_numpy(np.asarray(new_weights_matrix_3)))
    # model.fc4.weight.data.copy_(torch.from_numpy(np.asarray(new_weights_matrix_4)))

    #logits, log_vars = model(X)
    #loss = criterion(logits, y)
    #loss.backward()
    #optimizer.step()

    ####### Extract matrix from NN
    ####### Send matrix to master P

    to_send_1 = np.matrix(model.fc1.weight.data.cpu().numpy())
    to_send_2 = np.matrix(model.fc2.weight.data.cpu().numpy())
    to_send_3 = np.matrix(model.fc3.weight.data.cpu().numpy())
    to_send_4 = np.matrix(model.fc4.weight.data.cpu().numpy())

    client.send_msg("SENDING")
    # proto.send_matrix(to_send_1)
    # proto.send_matrix(to_send_2)
    # proto.send_matrix(to_send_3)
    # proto.send_matrix(to_send_4)