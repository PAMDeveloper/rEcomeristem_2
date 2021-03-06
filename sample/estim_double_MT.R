#Script estimating parameters for EcoMeristem model
#Authors : Florian Larue, Gregory Beurier, Lauriane Rouan, Delphine Luquet
#-- (PAM, AGAP, BIOS, CIRAD)

###GET ARGUMENTS (for cluster)
args = commandArgs(trailingOnly=TRUE)
if (length(args)==0) {
  stop("At least one argument must be supplied (path).n", call.=FALSE)
}

###START INSTALL AND PACKAGES LOAD###
.libPaths('/home/beurier/src/R/library')
library(recomeristem)
library(Rcpp)
library(parallel)
library(DEoptim)
library(data.table)

###SET INFORMATION FOR ESTIMATION###
FPath <- args[[1]]
nbCores <- 30
WorkDir <- "/homedir/beurier/work/"
DataDir <- paste(WorkDir,FPath,sep="")
DataDirStress <- paste(DataDir,"/stress", sep="")
vName <- "vobs_moy.txt"
vETName <- "vobs_et.txt"
paramOfInterest <- c("Epsib", "Ict","MGR_init","plasto_init","phyllo_init","ligulo_init",
                     "coef_MGR_PI","slope_length_IN","density_IN2","coef_plasto_PI",
                     "coef_phyllo_PI","coef_ligulo_PI","slope_LL_BL_at_PI","thresAssim",
                     "thresINER","thresLER","thresLEN","stressBP","stressBP2")
minValue <- c(3, 0.5, 6, 20, 20, 20, -0.5, 0.5, 0.08, 1, 1, 1, 0.0,1,1,1,1,0,1)
maxValue <- c(8, 2.5, 14, 45, 45, 45, 0.5, 1, 0.3, 3.0, 3.0, 3.0, 0.4,20,20,20,20,10,10)
coefIncrease <- 10
maxIter <- 10000
relTol <- 0.001 #estimation stops if unable to reduce RMSE by (reltol * rmse) after steptol steps
stepTol <- 10000
penalty <- 10 #Penalty for simulation outside of SD (RMSE * Penalty)
solTol <- 0.01 #will be multiplied by the number of observed variables
clusterA <- TRUE  #parallel for machines with at least 4 cores

###INIT SIMULATIONS###
meteo <- recomeristem::getMeteo_from_files(DataDir)
meteo_s <- recomeristem::getMeteo_from_files(DataDirStress)
param <- recomeristem::getParameters_from_files(DataDir)
obs <- recomeristem::get_clean_obs(paste(DataDir,"/",vName,sep=""))
obs_s <- recomeristem::get_clean_obs(paste(DataDirStress,"/",vName,sep=""))
obsET <- recomeristem::get_clean_obs(paste(DataDir,"/",vETName,sep=""))
obsET_s <- recomeristem::get_clean_obs(paste(DataDirStress,"/",vETName,sep=""))

###INIT ESTIMATION###
isInit <- FALSE
bounds <- matrix(c(minValue,maxValue),ncol=2)
obsCoef <- rep(1,ncol(obs))
obsCoef_s <- rep(1,ncol(obs_s))
nbParam <- length(paramOfInterest)
result <- list()
VarList <- names(obs)

###FUNCTIONS###
optimEcomeristem <- function(p) {
  if(isInit == FALSE) {
    recomeristem::init_simu(param, meteo, obs, "env1")
    recomeristem::init_simu(param, meteo_s, obs_s, "env2")
    isInit <<- TRUE
  }
  if("phyllo_init" %in% paramOfInterest && "plasto_init" %in% paramOfInterest && "ligulo_init" %in% paramOfInterest) {
    if(p[match("phyllo_init",paramOfInterest)] < p[match("plasto_init",paramOfInterest)]) {
      return(99999)
    } else if(p[match("ligulo_init",paramOfInterest)] < p[match("phyllo_init",paramOfInterest)]) {
      return(99999)
    } else if(p[match("phyllo_init",paramOfInterest)]*p[match("coef_phyllo_PI",paramOfInterest)] < p[match("plasto_init",paramOfInterest)]*p[match("coef_plasto_PI",paramOfInterest)]) {
      return(99999)
    } else if(p[match("ligulo_init",paramOfInterest)]*p[match("coef_ligulo_PI",paramOfInterest)] < p[match("phyllo_init",paramOfInterest)]*p[match("coef_phyllo_PI",paramOfInterest)]) {
      return(99999)
    }
  }
  res <- recomeristem::launch_simu("env1", paramOfInterest, p)
  res_s <- recomeristem::launch_simu("env2", paramOfInterest, p)

  diff1 <- abs(1-(data.table::between(res,obs-obsET,obs+obsET)))
  diff2 <- diff1 * penalty
  diff3 <-  replace(diff2, diff2 == 0,1)
  diff4 <- ((((obs - res)/obs)^2)*diff3)*coeff
  diff <- sum(sqrt((colSums(diff4, na.rm=T))/(colSums(!is.na(diff4)))),na.rm=T)

  diff1_s <- abs(1-(data.table::between(res_s,obs_s-obsET_s,obs_s+obsET_s)))
  diff2_s <- diff1_s * penalty
  diff3_s <-  replace(diff2_s, diff2_s == 0,1)
  diff4_s <- ((((obs_s - res_s)/obs_s)^2)*diff3_s)*coeff_s
  diff_s <- sum(sqrt((colSums(diff4_s, na.rm=T))/(colSums(!is.na(diff4_s)))),na.rm=T)

  return(diff+diff_s)
}
optimisation <- function(maxIter, solTol, relTol, stepTol, bounds) {
  if(clusterA && detectCores() >= 4) {
    resOptim <- DEoptim(optimEcomeristem, lower=bounds[,1], upper=bounds[,2], DEoptim.control(reltol=relTol,steptol=stepTol,VTR=solTol,itermax=maxIter,strategy=2,cluster=cl,packages=c("recomeristem"),parVar=c("meteo","meteo_s","obs_s","obsET_s","obs", "paramOfInterest", "obsET", "coeff_s","penalty","coeff","isInit","param")))
  } else {
    resOptim <- DEoptim(optimEcomeristem, lower=bounds[,1], upper=bounds[,2], DEoptim.control(reltol=relTol,steptol=stepTol,VTR=solTol,itermax=maxIter,strategy=2))
  }
  result$optimizer <- "Diffential Evolution Optimization"
  result$par <- resOptim$optim$bestmem
  result$value <- resOptim$optim$bestval
  result$iter <- resOptim$optim$iter
  return(list(result,resOptim))
}
coefCompute <- function(length,obsCoef) {
  tmp <- c()
  for(i in 0:(length-1)) {
    tmp <- c(tmp, (obsCoef+(i*(obsCoef/(100/coefIncrease)))))
  }
  return(data.frame(matrix(unlist(tmp),nrow=length,byrow=T)))
}

savePar <- function(name = Sys.Date()) {
  resPar <- matrix(as.vector(c(result$value, result$par)), ncol=length(paramOfInterest)+1)
  write.table(resPar, file=paste("par_",name,".csv"), sep=",", append=F, dec=".",col.names=c("RMSE",paramOfInterest),row.names = F)
}

#Optimisation run
coeff <<- coefCompute(nrow(obs),obsCoef)
coeff_s <<- coefCompute(nrow(obs_s),obsCoef_s)
set.seed(1337)
if(clusterA) {
  nbCores <- nbCores
  cl <- makeCluster(nbCores)
  clusterEvalQ(cl, library(recomeristem, lib.loc='/homedir/beurier/src/R/library'))
} #parallel
system.time(resOptim <- optimisation(maxIter, solTol, relTol, stepTol, bounds))
result <- resOptim[[1]]
savePar()
stopCluster(cl)
