
SRCS ?=
PROG ?=
BINDIR ?=
OBJDIR ?=

CC=gcc
CXX=g++
RM=rm -f
INCLUDES = -I../../ -I../ -I/usr/local/include -I../../fraser/src -I../../fraser -I../../models -I../../../cpp
CXXFLAGS := -std=c++1y -g -Wall -DBOOST_LOG_DYN_LINK ${INCLUDES} 
LDFLAGS = -L/usr/local/lib -L/usr/lib/x86_64-linux-gnu 
LIBS= -lzmq -lboost_serialization -lboost_system -lboost_filesystem -lboost_thread -lboost_log -lboost_log_setup -lpthread -lpugixml 

vpath %.cpp $(dir $(SRCS))

SRCNAMES = $(notdir $(SRCS))
OBJS = $(patsubst %.cpp, $(OBJDIR)/%.o, $(SRCNAMES))

$(BINDIR)/$(PROG): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $@ $(LIBS) $(LDFLAGS)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	@mkdir -p $@

$(OBJDIR)/%.o : %.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIBS) $(LDFLAGS)

.PHONY:
clean:
	$(RM) $(OBJS)
	$(RM) $(BINDIR)/$(PROG)