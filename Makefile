MYSQL_DIR = /home/jat/Softwares/mysql-connector-c-6.0.2

CXX = g++
CXXFLAGS = -O2 -g -I${MYSQL_DIR}/include
LDFLAGS = -L${MYSQL_DIR}/lib -lmysqlclient_r
OBJS = mysql_cpool.o test.o

a.out: $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm $(OBJS) a.out
