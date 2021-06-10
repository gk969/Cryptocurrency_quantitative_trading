import matplotlib.pyplot as plt

val=[]
val.append(0)
val2=[]
val2.append(0)
for i in range(1, 10000):
    val.append(val[i-1]+(1 / (val[i-1] + 1)))
    val2.append(val2[i-1]+(1 / (val2[i-1] + 10)))

plt.plot(val)
plt.plot(val2)
plt.show()


i_num=5
f_num=3.2
str_var="asd"
print(f"test {i_num} {f_num} {i_num+f_num} {str_var} {str(f_num)+str_var}")
