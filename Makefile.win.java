#
# NMake makefile for the LuaJava Windows Distribution
#

include makefile_config.win


#
# Other variables.
#
CLASSES     = \
	java/developpeur2000/luajitjava/LuaException.class \
	java/developpeur2000/luajitjava/LuaJitJavaAPI.class \
	
.SUFFIXES: .java .class

#
# Targets
#
run: build FORCE
	@echo ------------------
	@echo Build Complete
	@echo ------------------

build: checkjdk preclean $(CLASSES) $(JAR_FILE) clean FORCE

#
# Prebuild cleanliness.
#
preclean:
	-del $(JAR_FILE)

#
# Build .class files.
#
.java.class:
	"$(JDK)\bin\javac" -sourcepath ./java $*.java

#
# Create the JAR
#
$(JAR_FILE):
	cd java
	"$(JDK)\bin\jar" cvf ../bin/$(JAR_FILE) developpeur2000/luajitjava/*.class
	cd ..
  
#
# Check that the user has a valid JDK install.  This will cause a
# premature death if JDK is not defined.
#
checkjdk: "$(JDK)\bin\java.exe"

#
# Help deal with phony targets.
#
FORCE: ;

#
# Cleanliness.
#
clean:
	-del java\developpeur2000\luajitjava\*.class
